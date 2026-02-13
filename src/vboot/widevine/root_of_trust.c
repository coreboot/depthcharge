// SPDX-License-Identifier: GPL-2.0

#include <commonlib/helpers.h>
#include <endian.h>
#include <stdint.h>
#include <stdio.h>
#include <tlcl.h>
#include <tss_constants.h>
#include <vb2_api.h>

#include "arch/arm/smc.h"
#include "vboot/widevine.h"

/* Widevine unique device key index. */
#define WIDEVINE_NV_ROT_SEED_INDEX 0x3fff05

#define CROS_OEM_SMC_DRM_SET_HARDWARE_UNIQUE_KEY_FUNC_ID 0xC300C051
#define CROS_OEM_SMC_DRM_SET_ROOT_OF_TRUST_FUNC_ID 0xC300C052

#define CROS_OEM_QTEE_SMC_GSC_SET_ROOT_OF_TRUST_FUNC_ID 0x72000801
#define CROS_OEM_QTEE_SMC_DRM_SET_ROOT_OF_TRUST_FUNC_ID 0x72000802

#define WIDEVINE_SEED_LEN 32
/*
 * GSC NV storage layout for Widevine seeds.
 * Each seed is 32 bytes. The sequence is:
 *  1. RoT Seed     (offset 0)
 *  2. Reserved     (offset 32)
 *  3. Counter Key  (offset 64)
 */
struct widevine_nv_block {
	uint8_t rot_seed[WIDEVINE_SEED_LEN];
	uint8_t reserved[WIDEVINE_SEED_LEN];
	uint8_t counter_key[WIDEVINE_SEED_LEN];
};

static struct widevine_nv_block widevine_block_cache;
static bool widevine_cache_valid = false;

// Widevine RoT
static const uint8_t widevine_rot_context[] = {'W', 'i', 'd', 'e', 'v', 'i',
					       'n', 'e', ' ', 'R', 'o', 'T'};

// Widevine HUK
static const uint8_t widevine_huk_context[] = {'W', 'i', 'd', 'e', 'v', 'i',
					       'n', 'e', ' ', 'H', 'U', 'K'};

_Static_assert(CONFIG(WIDEVINE_PROVISION_OPTEE) !=
	           CONFIG(WIDEVINE_PROVISION_QTEE),
	           "Exactly one of WIDEVINE_PROVISION_OPTEE or WIDEVINE_PROVISION_QTEE must be set");

static uint32_t fill_widevine_cache(void)
{
	if (widevine_cache_valid)
		return TPM_SUCCESS;

	uint32_t ret = TlclRead(WIDEVINE_NV_ROT_SEED_INDEX, &widevine_block_cache,
			 sizeof(widevine_block_cache));

	if (ret == TPM_SUCCESS)
		widevine_cache_valid = true;
	else
		printf("GSC seed read failed: %#x\n", ret);

	return ret;
}

/**
 * Checks if a retrieved seed is valid.
 * Returns TPM_SUCCESS if valid, TPM_E_READ_EMPTY if all 00s or all FFs.
 */
static uint32_t validate_seed(const uint8_t *seed, size_t len, const char *name)
{
	uint8_t test_00 = 0;
	uint8_t test_ff = 0xff;

	for (size_t i = 0; i < len; i++) {
		test_00 |= seed[i];
		test_ff &= seed[i];
	}

	if (test_00 == 0 || test_ff == 0xff) {
		printf("Widevine %s seed is invalid (all %s)\n", name, test_ff ? "0xff" : "zeros");
		return TPM_E_READ_EMPTY;
	}

	return TPM_SUCCESS;
}

static uint32_t read_rot_seed(uint8_t *dest)
{
	uint32_t ret = fill_widevine_cache();
	if (ret != TPM_SUCCESS)
		return ret;

	memcpy(dest, widevine_block_cache.rot_seed, WIDEVINE_SEED_LEN);
	return validate_seed(dest, WIDEVINE_SEED_LEN, "RoT");
}

static uint32_t read_counter_key(uint8_t *dest)
{
	uint32_t ret = fill_widevine_cache();
	if (ret != TPM_SUCCESS)
		return ret;

	memcpy(dest, widevine_block_cache.counter_key, WIDEVINE_SEED_LEN);
	return validate_seed(dest, WIDEVINE_SEED_LEN, "Counter");
}

static void register_widevine_optee_huk(uint8_t huk[WIDEVINE_SEED_LEN])
{
	int ret;

	ret = widevine_write_smc_data(
		CROS_OEM_SMC_DRM_SET_HARDWARE_UNIQUE_KEY_FUNC_ID, huk,
		WIDEVINE_SEED_LEN);

	if (ret)
		printf("write TF-A widevine OPTEE HUK failed: %#x\n", ret);
}

static void register_widevine_optee_rot(uint8_t rot[WIDEVINE_SEED_LEN])
{
	int ret;

	ret = widevine_write_smc_data(
		CROS_OEM_SMC_DRM_SET_ROOT_OF_TRUST_FUNC_ID, rot,
		WIDEVINE_SEED_LEN);

	if (ret)
		printf("write TF-A widevine OPTEE ROT failed: %#x\n", ret);
}

static void register_widevine_qtee_seed(uint8_t seed[WIDEVINE_SEED_LEN], uint32_t func_id,
	 const char *name)
{
	size_t elements = WIDEVINE_SEED_LEN / sizeof(uint64_t);
	uint64_t qword_keys[elements];

	for (size_t index = 0; index < ARRAY_SIZE(qword_keys); index++)
		qword_keys[index] = le64dec(seed + index * 8);

	int ret = smc(func_id, elements,
		      qword_keys[0], qword_keys[1], qword_keys[2], qword_keys[3], 0);

	if (ret)
		printf("write TF-A widevine QTEE %s failed: %#x\n", name, ret);
}

static void register_widevine_qtee_rot(uint8_t rot[WIDEVINE_SEED_LEN])
{
	register_widevine_qtee_seed(rot, CROS_OEM_QTEE_SMC_DRM_SET_ROOT_OF_TRUST_FUNC_ID, "ROT");
}

static void register_widevine_qtee_counter(uint8_t counter[WIDEVINE_SEED_LEN])
{
	register_widevine_qtee_seed(counter, CROS_OEM_QTEE_SMC_GSC_SET_ROOT_OF_TRUST_FUNC_ID,
			 "Counter");
}

uint32_t prepare_widevine_root_of_trust(struct vb2_context *ctx)
{
	uint8_t rot_seed[WIDEVINE_SEED_LEN];
	struct vb2_hash rot;
	struct vb2_hash huk;
	uint32_t ret;

	ret = read_rot_seed(rot_seed);
	if (ret != TPM_SUCCESS) {
		printf("read_rot_seed() failed: %#x\n", ret);
		/* Continue the the zero UDS for the development process. */
		memset(rot_seed, 0, WIDEVINE_SEED_LEN);
	}

	if (vb2_hmac_calculate(vb2api_hwcrypto_allowed(ctx), VB2_HASH_SHA256,
			       rot_seed, sizeof(rot_seed), widevine_rot_context,
			       sizeof(widevine_rot_context), &rot)) {
		printf("Failed to calculate hmac for RoT\n");
		return TPM_E_INTERNAL_ERROR;
	}

	if (CONFIG(WIDEVINE_PROVISION_OPTEE)) {
		if (vb2_hmac_calculate(vb2api_hwcrypto_allowed(ctx), VB2_HASH_SHA256,
				       rot_seed, sizeof(rot_seed), widevine_huk_context,
				       sizeof(widevine_huk_context), &huk)) {
			printf("Failed to calculate hmac for HUK\n");
			return TPM_E_INTERNAL_ERROR;
		}
		register_widevine_optee_huk(huk.sha256);
		register_widevine_optee_rot(rot.sha256);
	} else {
		register_widevine_qtee_rot(rot.sha256);
	}

	/* Need special handing for non-CrOS devices */
	if (CONFIG(WIDEVINE_PROVISION_LEGACY))
		return TPM_SUCCESS;

	uint8_t counter_key[WIDEVINE_SEED_LEN];

	ret = read_counter_key(counter_key);
	if (ret != TPM_SUCCESS) {
		printf("read_counter_key() failed: %#x\n", ret);
		/* Continue the the zero UDS for the development process. */
		memset(counter_key, 0, WIDEVINE_SEED_LEN);
	}

	if (CONFIG(WIDEVINE_PROVISION_QTEE))
		register_widevine_qtee_counter(counter_key);

	return TPM_SUCCESS;
}

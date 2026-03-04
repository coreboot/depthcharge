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
#define CROS_OEM_SMC_DRM_SET_GSC_COUNTER_KEY_FUNC_ID 0xC300C053
#define CROS_OEM_SMC_DRM_SET_DRM_DEVICE_KEY_FUNC_ID 0xC300C054
#define CROS_OEM_SMC_DRM_SET_STABLE_HARDWARE_UNIQUE_KEY_FUNC_ID 0xC300C055

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
	uint8_t stable_seed[WIDEVINE_SEED_LEN];
	uint8_t counter_key[WIDEVINE_SEED_LEN];
};

// Widevine RoT
static const uint8_t widevine_rot_context[] = {'W', 'i', 'd', 'e', 'v', 'i',
					       'n', 'e', ' ', 'R', 'o', 'T'};

// Widevine HUK
static const uint8_t widevine_huk_context[] = {'W', 'i', 'd', 'e', 'v', 'i',
					       'n', 'e', ' ', 'H', 'U', 'K'};

#define WIDEVINE_STABLE_SEED_VERSION_CONTEXT '1'

// Widevine new HUK (for stable seed)
static const uint8_t widevine_stable_huk_context[] = {'W', 'i', 'd', 'e', 'v',
						   'i', 'n', 'e', ' ', 'H', 'U', 'K',
						   WIDEVINE_STABLE_SEED_VERSION_CONTEXT};
// Widevine device key
static const uint8_t widevine_device_key_context[] = {'W', 'i', 'd', 'e', 'v',
					       'i', 'n', 'e', ' ', 'D', 'V', 'K',
						   WIDEVINE_STABLE_SEED_VERSION_CONTEXT};

_Static_assert(CONFIG(WIDEVINE_PROVISION_OPTEE) !=
		   CONFIG(WIDEVINE_PROVISION_QTEE),
		   "Exactly one of WIDEVINE_PROVISION_OPTEE or WIDEVINE_PROVISION_QTEE must be set");

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
		printf("Widevine %s seed is invalid (all %s)\n", name,
			test_ff ? "0xff" : "zeros");
		return TPM_E_READ_EMPTY;
	}

	return TPM_SUCCESS;
}

static uint32_t read_seeds(struct widevine_nv_block *block)
{
	uint32_t ret = TlclRead(WIDEVINE_NV_ROT_SEED_INDEX, block, sizeof(*block));

	if (ret != TPM_SUCCESS) {
		printf("GSC seed read failed: %#x\n", ret);
		return ret;
	}

	ret = validate_seed(block->rot_seed, WIDEVINE_SEED_LEN, "RoT");
	if (ret != TPM_SUCCESS)
		return ret;

	ret = validate_seed(block->stable_seed, WIDEVINE_SEED_LEN, "Stable");
	if (ret != TPM_SUCCESS)
		return ret;

	ret = validate_seed(block->counter_key, WIDEVINE_SEED_LEN, "Counter");
	if (ret != TPM_SUCCESS)
		return ret;
	return TPM_SUCCESS;
}

static void register_widevine_optee_data(uint8_t data[WIDEVINE_SEED_LEN],
				   uint32_t func_id, const char *name)
{
	int ret;
	ret = widevine_write_smc_data(func_id, data, WIDEVINE_SEED_LEN);

	if (ret)
		printf("write TF-A widevine OPTEE %s failed: %#x\n", name, ret);
}

static void register_widevine_qtee_data(uint8_t seed[WIDEVINE_SEED_LEN],
				   uint32_t func_id, const char *name)
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

static uint32_t derive_and_register_seed_optee(struct vb2_context *ctx,
				   uint8_t *seed, const uint8_t *context, size_t context_len,
				   uint32_t func_id, const char *name)
{
	struct vb2_hash hash;
	if (vb2_hmac_calculate(vb2api_hwcrypto_allowed(ctx), VB2_HASH_SHA256,
			       seed, WIDEVINE_SEED_LEN,
			       context, context_len, &hash)) {
		printf("Failed to calculate hmac for %s\n", name);
		return TPM_E_INTERNAL_ERROR;
	}

	register_widevine_optee_data(hash.sha256, func_id, name);
	return TPM_SUCCESS;
}

static uint32_t derive_and_register_seed_qtee(struct vb2_context *ctx,
				   uint8_t *seed, const uint8_t *context, size_t context_len,
				   uint32_t func_id, const char *name)
{
	struct vb2_hash hash;
	if (vb2_hmac_calculate(vb2api_hwcrypto_allowed(ctx), VB2_HASH_SHA256,
			       seed, WIDEVINE_SEED_LEN,
				   context, context_len, &hash)) {
		printf("Failed to calculate hmac for %s\n", name);
		return TPM_E_INTERNAL_ERROR;
	}

	register_widevine_qtee_data(hash.sha256, func_id, name);
	return TPM_SUCCESS;
}

uint32_t prepare_optee_root_of_trust(struct vb2_context *ctx,
					struct widevine_nv_block *block)
{
	if (derive_and_register_seed_optee(ctx, block->rot_seed,
				   widevine_rot_context, sizeof(widevine_rot_context),
				   CROS_OEM_SMC_DRM_SET_ROOT_OF_TRUST_FUNC_ID, "ROT")) {
		return TPM_E_INTERNAL_ERROR;
	}
	if (derive_and_register_seed_optee(ctx, block->rot_seed,
				   widevine_huk_context, sizeof(widevine_huk_context),
				   CROS_OEM_SMC_DRM_SET_HARDWARE_UNIQUE_KEY_FUNC_ID, "HUK")) {
		return TPM_E_INTERNAL_ERROR;
	}
	if (derive_and_register_seed_optee(ctx, block->stable_seed,
				   widevine_stable_huk_context,
				   sizeof(widevine_stable_huk_context),
				   CROS_OEM_SMC_DRM_SET_STABLE_HARDWARE_UNIQUE_KEY_FUNC_ID,
				   "Stable HUK")) {
		return TPM_E_INTERNAL_ERROR;
	}
	if (derive_and_register_seed_optee(ctx, block->stable_seed,
				   widevine_device_key_context,
				   sizeof(widevine_device_key_context),
				   CROS_OEM_SMC_DRM_SET_DRM_DEVICE_KEY_FUNC_ID,
				   "Device Key")) {
		return TPM_E_INTERNAL_ERROR;
	}
	register_widevine_optee_data(block->counter_key,
				     CROS_OEM_SMC_DRM_SET_GSC_COUNTER_KEY_FUNC_ID,
				     "Counter Key");
	return TPM_SUCCESS;
}

uint32_t prepare_qtee_root_of_trust(struct vb2_context *ctx,
				       struct widevine_nv_block *block)
{
	if (derive_and_register_seed_qtee(ctx, block->rot_seed,
				   widevine_rot_context, sizeof(widevine_rot_context),
				   CROS_OEM_QTEE_SMC_DRM_SET_ROOT_OF_TRUST_FUNC_ID, "ROT")) {
		return TPM_E_INTERNAL_ERROR;
	}
	register_widevine_qtee_data(block->counter_key,
				      CROS_OEM_QTEE_SMC_GSC_SET_ROOT_OF_TRUST_FUNC_ID,
				     "Counter Key");
	return TPM_SUCCESS;
}

uint32_t prepare_widevine_root_of_trust(struct vb2_context *ctx)
{
	struct widevine_nv_block block;
	uint32_t ret;

	ret = read_seeds(&block);
	if (ret != TPM_SUCCESS) {
		printf("read_seeds() failed: %#x\n", ret);
		/* Continue the the zero UDS for the development process. */
		memset(block.rot_seed, 0, WIDEVINE_SEED_LEN);
		memset(block.stable_seed, 0, WIDEVINE_SEED_LEN);
		memset(block.counter_key, 0, WIDEVINE_SEED_LEN);
	}

	if (CONFIG(WIDEVINE_PROVISION_OPTEE))
		ret = prepare_optee_root_of_trust(ctx, &block);
	else
		ret = prepare_qtee_root_of_trust(ctx, &block);

	return ret;
}

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

#define WIDEVINE_NV_ROT_SEED_LEN 32

#define WIDEVINE_HUK_LEN 32
#define WIDEVINE_ROT_LEN 32

#define CROS_OEM_SMC_DRM_SET_HARDWARE_UNIQUE_KEY_FUNC_ID 0xC300C051
#define CROS_OEM_SMC_DRM_SET_ROOT_OF_TRUST_FUNC_ID 0xC300C052

#define CROS_OEM_QTEE_SMC_DRM_SET_ROOT_OF_TRUST_FUNC_ID 0x72000802

// Widevine RoT
static const uint8_t widevine_rot_context[] = {'W', 'i', 'd', 'e', 'v', 'i',
					       'n', 'e', ' ', 'R', 'o', 'T'};

// Widevine HUK
static const uint8_t widevine_huk_context[] = {'W', 'i', 'd', 'e', 'v', 'i',
					       'n', 'e', ' ', 'H', 'U', 'K'};

_Static_assert(CONFIG(WIDEVINE_PROVISION_OPTEE) !=
	           CONFIG(WIDEVINE_PROVISION_QTEE),
	           "Exactly one of WIDEVINE_PROVISION_OPTEE or WIDEVINE_PROVISION_QTEE must be set");

static uint32_t read_rot_seed(uint8_t *rot_seed)
{
	uint32_t ret;
	uint8_t test_byte = 0;
	uint8_t test_byte_ff = 0xff;
	int i;

	ret = TlclRead(WIDEVINE_NV_ROT_SEED_INDEX, rot_seed,
		       WIDEVINE_NV_ROT_SEED_LEN);

	if (ret != TPM_SUCCESS) {
		printf("read RoT seed failed: %#x\n", ret);
		return ret;
	}

	/* The RoT seed should not be all 0xff or 0x0. */
	for (i = 0; i < WIDEVINE_NV_ROT_SEED_LEN; i++) {
		test_byte |= rot_seed[i];
		test_byte_ff &= rot_seed[i];
	}

	if (test_byte == 0 || test_byte_ff == 0xff) {
		printf("Empty RoT seed\n");
		return TPM_E_READ_EMPTY;
	}

	return TPM_SUCCESS;
}

static void register_widevine_optee_huk(uint8_t huk[WIDEVINE_HUK_LEN])
{
	int ret;

	ret = widevine_write_smc_data(
		CROS_OEM_SMC_DRM_SET_HARDWARE_UNIQUE_KEY_FUNC_ID, huk,
		WIDEVINE_HUK_LEN);

	if (ret)
		printf("write TF-A widevine OPTEE HUK failed: %#x\n", ret);
}

static void register_widevine_optee_rot(uint8_t rot[WIDEVINE_ROT_LEN])
{
	int ret;

	ret = widevine_write_smc_data(
		CROS_OEM_SMC_DRM_SET_ROOT_OF_TRUST_FUNC_ID, rot,
		WIDEVINE_ROT_LEN);

	if (ret)
		printf("write TF-A widevine OPTEE ROT failed: %#x\n", ret);
}

static void register_widevine_qtee_rot(uint8_t rot[WIDEVINE_ROT_LEN])
{
	size_t elements = WIDEVINE_ROT_LEN / sizeof(uint64_t);
	uint64_t qword_keys[elements];

	for (size_t index = 0; index < ARRAY_SIZE(qword_keys); index++)
		qword_keys[index] = le64dec(rot + index * 8);

	int ret = smc(CROS_OEM_QTEE_SMC_DRM_SET_ROOT_OF_TRUST_FUNC_ID, elements,
		      qword_keys[0], qword_keys[1], qword_keys[2], qword_keys[3], 0);

	if (ret)
		printf("write TF-A widevine QTEE ROT failed: %#x\n", ret);
}

uint32_t prepare_widevine_root_of_trust(struct vb2_context *ctx)
{
	uint8_t rot_seed[WIDEVINE_NV_ROT_SEED_LEN];
	struct vb2_hash rot;
	struct vb2_hash huk;
	uint32_t ret;

	ret = read_rot_seed(rot_seed);
	if (ret != TPM_SUCCESS) {
		printf("read_rot_seed() failed: %#x\n", ret);
		/* Continue the the zero UDS for the development process. */
		memset(rot_seed, 0, WIDEVINE_NV_ROT_SEED_LEN);
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

	return TPM_SUCCESS;
}

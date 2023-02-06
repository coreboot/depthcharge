// SPDX-License-Identifier: GPL-2.0

#include <assert.h>
#include <endian.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <tlcl.h>
#include <tss_constants.h>

#include "vboot/widevine.h"

/* Widevine Auth public key index. */
#define WIDEVINE_AUTH_PUBKEY_INDEX 0x81000005

#define CMD_BUFFER_SIZE 512

#define CROS_OEM_SMC_DRM_SET_TPM_AUTH_PUB_FUNC_ID 0xC300C050

/* The public part of the widevine salt key. */
static const struct __packed {
	uint16_t type;
	uint16_t name_alg;
	uint32_t object_attributes;
	struct __packed {
		uint16_t size;
		uint8_t buffer[32];
	} auth_policy;
	union {
		struct __packed {
			uint16_t symmetric;
			uint16_t scheme;
			uint16_t curve_id;
			uint16_t kdf;
		} ecc_detail;
	} parameters;
	union {
		struct __packed {
			struct __packed {
				uint16_t size;
			} x;
			struct __packed {
				uint16_t size;
			} y;
		} ecc;
	} unique;
} widevine_salting_key_public = {
	/* TPM_ALG_ECC */
	.type = htobe16(0x0023),
	/* TPM_ALG_SHA256 */
	.name_alg = htobe16(0x000b),
	/* kFixedTPM | kFixedParent |  kDecrypt | kSensitiveDataOrigin |
	   kUserWithAuth | kNoDA */
	.object_attributes = htobe32(0x000204b2),
	.auth_policy = {
		/* Size of SHA256 */
		.size = htobe16(0x0020),
		/* SHA256("Widevine Salting Key") */
		.buffer = {
			0xe1, 0x47, 0xbf, 0x27, 0xe1, 0x74, 0x30, 0xc8,
			0x16, 0xab, 0x72, 0x4d, 0x5c, 0x77, 0xe1, 0x5c,
			0x61, 0x2d, 0x56, 0x81, 0xb3, 0x35, 0xcd, 0x9d,
			0xeb, 0x67, 0x41, 0x37, 0x69, 0xf0, 0x32, 0x41,
		},
	},
	.parameters.ecc_detail = {
		/* TPM_ALG_NULL */
		.symmetric = htobe16(0x0010),
		/* TPM_ALG_NULL */
		.scheme = htobe16(0x0010),
		/* TPM_ECC_NIST_P256 */
		.curve_id = htobe16(0x0003),
		/* TPM_ALG_NULL */
		.kdf = htobe16(0x0010),
	},
	.unique.ecc = {
		.x.size = htobe16(0),
		.y.size = htobe16(0),
	},
};

_Static_assert(sizeof(widevine_salting_key_public) == 54,
	       "Wrong size of salting key public");

static uint32_t register_tpm_auth_public_key(void)
{
	uint8_t pub_buffer[CMD_BUFFER_SIZE];
	uint32_t pub_len;
	uint32_t ret;

	pub_len = CMD_BUFFER_SIZE;
	ret = TlclReadPublic(WIDEVINE_AUTH_PUBKEY_INDEX, pub_buffer, &pub_len);

	if (ret != TPM_SUCCESS) {
		printf("tlcl_read_public() failed: %#x\n", ret);
		return ret;
	}

	if (pub_len < offsetof(typeof(widevine_salting_key_public), unique)) {
		printf("tpm auth public key is too small\n");
		return TPM_E_INPUT_TOO_SMALL;
	}

	/*
	 * Make sure the key is generated from the known template.
	 * For example, we would not want to use a key that wad imported to TPM
	 * from userland.
	 */
	if (memcmp(pub_buffer, &widevine_salting_key_public,
		   offsetof(typeof(widevine_salting_key_public), unique))) {
		printf("tpm auth public key doesn't match the template\n");
		return TPM_E_CORRUPTED_STATE;
	}

	ret = widevine_write_smc_data(CROS_OEM_SMC_DRM_SET_TPM_AUTH_PUB_FUNC_ID,
				      pub_buffer, pub_len);
	if (ret) {
		printf("write TF-A tpm auth public key failed: %#x\n", ret);
		return TPM_E_WRITE_FAILURE;
	}

	return TPM_SUCCESS;
}

static uint32_t create_tpm_auth_public_key(void)
{
	TPM_PERMANENT_FLAGS pflags;
	uint32_t ret;
	uint32_t temp_handle;

	ret = TlclGetPermanentFlags(&pflags);
	if (ret) {
		printf("tpm get capability failed: %#x\n", ret);
		return ret;
	}

	if (pflags.ownerAuthSet) {
		printf("cannot create tpm key when owner auth is set\n");
		return TPM_E_OWNER_SET;
	}

	if (pflags.endorsementAuthSet) {
		printf("cannot create tpm key when endorsement auth is set\n");
		return TPM_E_OWNER_SET;
	}

	ret = TlclCreatePrimary(
		TPM_RH_ENDORSEMENT, &widevine_salting_key_public,
		sizeof(widevine_salting_key_public), &temp_handle);

	if (ret) {
		printf("create tpm key failed: %#x\n", ret);
		return ret;
	}

	ret = TlclEvictControl(TPM_RH_OWNER, temp_handle,
			       WIDEVINE_AUTH_PUBKEY_INDEX);
	if (ret) {
		printf("persist tpm key failed: %#x\n", ret);
		return ret;
	}

	return TPM_SUCCESS;
}

uint32_t prepare_widevine_tpm_pubkey(void)
{
	uint32_t ret;

	ret = register_tpm_auth_public_key();

	if (ret != TPM_E_BADINDEX)
		return ret;

	printf("TPM auth public key not found, try to create a new one\n");

	ret = create_tpm_auth_public_key();
	if (ret != TPM_SUCCESS) {
		printf("create_tpm_auth_public_key() failed: %#x\n", ret);
		return ret;
	}

	ret = register_tpm_auth_public_key();
	if (ret != TPM_SUCCESS) {
		printf("register_tpm_auth_public_key() failed: %#x\n", ret);
		return ret;
	}

	return TPM_SUCCESS;
}

// SPDX-License-Identifier: GPL-2.0

#include <commonlib/helpers.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "boot/android_pvmfw.h"

#define ANDROID_PVMFW_CFG_MAGIC "pvmf"
#define ANDROID_PVMFW_CFG_V1_2 ((1 << 16) | (2 & 0xffff))
#define ANDROID_PVMFW_CFG_BLOB_ALIGN 8

struct pvmfw_config_entry {
	uint32_t offset;
	uint32_t size;
};

struct pvmfw_config_v1_2 {
	/* Header */
	uint8_t magic[4];
	uint32_t version;
	uint32_t total_size;
	uint32_t flags;

	/* Entry 0: AndroidDiceHandover CBOR */
	struct pvmfw_config_entry dice_handover;
	/* Entry 1: Debug Policy DTBO */
	struct pvmfw_config_entry dp_dtbo;
	/* Entry 2: VM Device assignment DTBO */
	struct pvmfw_config_entry da_dtbo;
	/* Entry 3: VM reference DTB */
	struct pvmfw_config_entry ref_dtb;

	/* Entries' blobs */
	uint8_t blobs[];
};

/**
 * BootParam CBOR object flatten into raw C structure.
 */
struct pvmfw_boot_params_cbor_v1 {
	/*
	 * 0xA3 - CBOR Map 3 elements
	 * 0x01 - uint = 1		Version label
	 * 0x00 - uint = 0		Version
	 * 0x02 - uint = 2		Boot parameters label
	 */
	uint8_t cbor_labels_preamble[4];
	struct {
		/*
		 * 0xA3 - CBOR Map 3 elements
		 * 0x01 - uint = 1	EarlyEntropy label
		 * 0x58 - bstr
		 * 0x40 - .size 64
		 */
		uint8_t cbor_labels_entropy[4];
		uint8_t early_entropy[64];
		/*
		 * 0x02 - uint = 2	SessionKeySeed label
		 * 0x58 - bstr
		 * 0x20 - .size 32
		 */
		uint8_t cbor_labels_session[3];
		uint8_t session_key_seed[32];
		/*
		 * 0x03 - uint = 3	AuthTokenKeySeed label
		 * 0x58 - bstr
		 * 0x20 - .size 32
		 */
		uint8_t cbor_labels_auth[3];
		uint8_t auth_token_key_seed[32];
	} boot_params;

	/*
	 * 0x03 - uint = 3	AndroidDiceHandover label
	 */
	uint8_t cbor_label_handover[1];

	/* AndroidDiceHandover */
	uint8_t android_dice_handover[];
} __attribute__((packed));

/**
 * Parse BootParam CBOR object and extract relevant data.
 */
static int parse_boot_params(const void *blob, size_t size, struct pvmfw_config_v1_3 *cfg,
			     size_t max_blobs_size)
{
	const char *field;
	struct pvmfw_boot_params_cbor_v1 *v1 = (void *)blob;

	/*
	 * Verify that the blob is at least big enough to compare the
	 * constant fragments.
	 */
	if (size < sizeof(struct pvmfw_boot_params_cbor_v1)) {
		printf("Received BootParams CBOR is too short.\n");
		return -1;
	}

	/*
	 * Verify that the known constant CBOR fragment match the expected
	 * values. The following constant byte strings are described in
	 * pvmfw_boot_params_cbor_v1 comments.
	 */
	static const uint8_t cbor_labels_preamble[] = {0xA3, 0x01, 0x00, 0x02};
	if (memcmp(v1->cbor_labels_preamble, cbor_labels_preamble,
		   sizeof(cbor_labels_preamble))) {
		field = "preamble (version)";
		goto mismatch;
	}

	static const uint8_t cbor_labels_entropy[] = {0xA3, 0x01, 0x58, 0x40};
	if (memcmp(v1->boot_params.cbor_labels_entropy, cbor_labels_entropy,
		   sizeof(cbor_labels_entropy))) {
		field = "early_entropy";
		goto mismatch;
	}

	static const uint8_t cbor_labels_session[] = {0x02, 0x58, 0x20};
	if (memcmp(v1->boot_params.cbor_labels_session, cbor_labels_session,
		   sizeof(cbor_labels_session))) {
		field = "session_key_seed";
		goto mismatch;
	}

	static const uint8_t cbor_labels_auth[] = {0x03, 0x58, 0x20};
	if (memcmp(v1->boot_params.cbor_labels_auth, cbor_labels_auth,
		   sizeof(cbor_labels_auth))) {
		field = "auth_token_key_seed";
		goto mismatch;
	}

	if (v1->cbor_label_handover[0] != 0x03) {
		field = "android_dice_handover";
		goto mismatch;
	}

	/*
	 * Verified that the blob matches the expected structure.
	 *
	 * The remaining bytes of BootParams structure are AndroidDiceHandover
	 * bytes.
	 *
	 * Fill the entry 0 location. The remaining entries are
	 * not used so leave them as zeros.
	 *
	 * Place handover right after config header.
	 */
	cfg->dice_handover.offset = offsetof(struct pvmfw_config_v1_3, blobs);
	cfg->dice_handover.size = size - sizeof(struct pvmfw_boot_params_cbor_v1);

	/* Make sure that there is enough space for handover in the buffer */
	if (cfg->dice_handover.size > max_blobs_size) {
		printf("Not enough space in the destination buffer for handover data.\n");
		return -1;
	}

	/* Copy handover data into place. */
	memcpy(&cfg->blobs[0], v1->android_dice_handover, cfg->dice_handover.size);

	/* Calculate the total size of the config with blobs, using the last used entry */
	cfg->total_size = cfg->dice_handover.offset + cfg->dice_handover.size;

	return 0;
mismatch:
	printf("Failed to parse BootParams CBOR structure. Constant fragment "
	       "around '%s' mismatches expected structure.\n",
	       field);
	return 1;
}

int setup_android_pvmfw(void *buffer, size_t buffer_size, size_t *pvmfw_size,
			const void *params, size_t params_size)
{
	int ret;
	uintptr_t pvmfw_addr, pvmfw_max, cfg_addr;
	struct pvmfw_config_v1_2 *cfg;

	/* Set the boundaries of the pvmfw buffer */
	pvmfw_addr = (uintptr_t)buffer;
	pvmfw_max = pvmfw_addr + buffer_size;

	/* Align to where pvmfw will search for the config header */
	cfg_addr = ALIGN_UP(pvmfw_addr + *pvmfw_size, ANDROID_PVMFW_CFG_ALIGN);
	cfg = (void *)cfg_addr;

	/* Make sure we have space for config and for some blobs */
	if ((uintptr_t)cfg->blobs >= pvmfw_max) {
		printf("No space in the buffer for pvmfw config\n");
		ret = -1;
		goto fail;
	}

	/* Fill the header */
	memset(cfg, 0, sizeof(*cfg));
	memcpy(cfg->magic, ANDROID_PVMFW_CFG_MAGIC, sizeof(cfg->magic));
	cfg->version = ANDROID_PVMFW_CFG_V1_3;

	/* Parse and extract data from BootParam CBOR object */
	ret = parse_boot_params(params, params_size, cfg,
				(size_t)(pvmfw_max - (uintptr_t)cfg->blobs));
	if (ret != 0) {
		printf("Failed to parse pvmfw gsc boot params data\n");
		goto fail;
	}

	/*
	 * Truncate the pvmfw's avb footer to minimize reserved memory size
	 * aligned up to config alignment.
	 */
	*pvmfw_size =
		ALIGN_UP(cfg_addr - pvmfw_addr + cfg->total_size, ANDROID_PVMFW_CFG_ALIGN);

	return 0;
fail:
	*pvmfw_size = 0;
	return ret;
}

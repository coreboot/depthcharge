// SPDX-License-Identifier: GPL-2.0

#include <commonlib/helpers.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "boot/android_pvmfw.h"

#define ANDROID_PVMFW_CFG_MAGIC "pvmf"
#define ANDROID_PVMFW_CFG_V1_3 ((1 << 16) | (3 & 0xffff))
#define ANDROID_PVMFW_CFG_BLOB_ALIGN 8

struct pvmfw_config_entry {
	uint32_t offset;
	uint32_t size;
} __attribute__((packed));

struct pvmfw_config_v1_3 {
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
	/* Entry 4: VM reserved memory blobs */
	struct pvmfw_config_entry reserved_mem;

	/* Entries' blobs */
	uint8_t blobs[];
} __attribute__((packed));

/* pvmfw's entry 4: Reserved memory blob header */
struct pvmfw_cfg_rmem_hdr {
	uint8_t vm_uuid[16];
	uint32_t blob_offset;
	uint32_t blob_size;
	uint32_t compat_offset;

#define PVMFW_CFG_RESERVED_MEM_NO_MAP 0x1
	uint32_t flags;
} __attribute__((packed));

/* UUID: 0591f706-535b-4472-b8ea-41535f5b1169 */
#define DESKTOP_TRUSTY_VM_UUID                                                                 \
	{                                                                                      \
		0x05, 0x91, 0xf7, 0x06, 0x53, 0x5b, 0x44, 0x72,                                \
		0xb8, 0xea, 0x41, 0x53, 0x5f, 0x5b, 0x11, 0x69,                                \
	}

#define GSC_EARLY_ENTROPY_SIZE 64
#define GSC_SESSION_KEY_SEED_SIZE 32
#define GSC_AUTH_TOKEN_SIZE 32

static const char entropy_compat[] = "google,early-entropy";
static const char session_compat[] = "google,session-key-seed";
static const char auth_token_compat[] = "google,auth-token-key-seed";

/* pvmfw's entry 4: Reserved memory blobs with headers */
struct pvmfw_cfg_rmem {
	uint32_t count;

#define PVMFW_CFG_RESERVED_MEM_BLOB_COUNT 3
	struct {
		struct pvmfw_cfg_rmem_hdr entropy;
		struct pvmfw_cfg_rmem_hdr session;
		struct pvmfw_cfg_rmem_hdr auth;
	} hdrs;

	struct reserved_blobs {
		uint8_t early_entropy[GSC_EARLY_ENTROPY_SIZE];
		uint8_t entropy_compat[sizeof(entropy_compat)];

		uint8_t session_key_seed[GSC_SESSION_KEY_SEED_SIZE];
		uint8_t session_compat[sizeof(session_compat)];

		uint8_t auth_token_key_seed[GSC_AUTH_TOKEN_SIZE];
		uint8_t auth_token_compat[sizeof(auth_token_compat)];
	} blobs;
} __attribute__((packed));

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
		uint8_t early_entropy[GSC_EARLY_ENTROPY_SIZE];
		/*
		 * 0x02 - uint = 2	SessionKeySeed label
		 * 0x58 - bstr
		 * 0x20 - .size 32
		 */
		uint8_t cbor_labels_session[3];
		uint8_t session_key_seed[GSC_SESSION_KEY_SEED_SIZE];
		/*
		 * 0x02 - uint = 3	AuthTokenKeySeed label
		 * 0x58 - bstr
		 * 0x20 - .size 32
		 */
		uint8_t cbor_labels_auth[3];
		uint8_t auth_token_key_seed[GSC_AUTH_TOKEN_SIZE];
	} boot_params;

	/*
	 * 0x03 - uint = 3	AndroidDiceHandover label
	 */
	uint8_t cbor_label_handover[1];

	/* AndroidDiceHandover */
	uint8_t android_dice_handover[];
} __attribute__((packed));

/**
 * Offsets and sizes of extracted data in the buffer.
 */
struct pvmfw_boot_params {
	/* Entry 0: AndroidDiceHandover offset and size */
	size_t handover_offset;
	size_t handover_size;

	/* Entry 4: Reserved memory offset and sizes */
	size_t reserved_mem_offset;
	/* Size of entire entry with the headers and also blobs */
	size_t reserved_mem_size;
};

static void copy_reserved_mem(struct pvmfw_boot_params_cbor_v1 *v1,
			      struct pvmfw_cfg_rmem *reserved)
{
	struct reserved_blobs *blobs = &reserved->blobs;

	/* Clear the size of entry that will be used */
	memset(reserved, 0, sizeof(struct pvmfw_cfg_rmem));

	/* Set the count of the reserved memory entiries */
	reserved->count = PVMFW_CFG_RESERVED_MEM_BLOB_COUNT;

	/* Setup the reserved memory header for early entropy */
	reserved->hdrs.entropy = (struct pvmfw_cfg_rmem_hdr){
		.vm_uuid = DESKTOP_TRUSTY_VM_UUID,
		.blob_offset = offsetof(struct reserved_blobs, early_entropy),
		.blob_size = GSC_EARLY_ENTROPY_SIZE,
		.compat_offset = offsetof(struct reserved_blobs, entropy_compat),
		.flags = PVMFW_CFG_RESERVED_MEM_NO_MAP,
	};

	/* Copy early entropy received from GSC to reserved memory blobs */
	memcpy(blobs->early_entropy, v1->boot_params.early_entropy, GSC_EARLY_ENTROPY_SIZE);
	memcpy(blobs->entropy_compat, entropy_compat, sizeof(entropy_compat));

	/* Setup the reserved memory header for session key seed */
	reserved->hdrs.session = (struct pvmfw_cfg_rmem_hdr){
		.vm_uuid = DESKTOP_TRUSTY_VM_UUID,
		.blob_offset = offsetof(struct reserved_blobs, session_key_seed),
		.blob_size = GSC_SESSION_KEY_SEED_SIZE,
		.compat_offset = offsetof(struct reserved_blobs, session_compat),
		.flags = PVMFW_CFG_RESERVED_MEM_NO_MAP,
	};

	/* Copy session key seed received from GSC to reserved memory blobs */
	memcpy(blobs->session_key_seed, v1->boot_params.session_key_seed,
	       GSC_SESSION_KEY_SEED_SIZE);
	memcpy(blobs->session_compat, session_compat, sizeof(session_compat));

	/* Setup the reserved memory header for auth token key seed */
	reserved->hdrs.auth = (struct pvmfw_cfg_rmem_hdr){
		.vm_uuid = DESKTOP_TRUSTY_VM_UUID,
		.blob_offset = offsetof(struct reserved_blobs, auth_token_key_seed),
		.blob_size = GSC_AUTH_TOKEN_SIZE,
		.compat_offset = offsetof(struct reserved_blobs, auth_token_compat),
		.flags = PVMFW_CFG_RESERVED_MEM_NO_MAP,
	};

	/* Copy auth token key seed received from GSC to first reserved memory */
	memcpy(blobs->auth_token_key_seed, v1->boot_params.auth_token_key_seed,
	       GSC_AUTH_TOKEN_SIZE);
	memcpy(blobs->auth_token_compat, auth_token_compat, sizeof(auth_token_compat));
}

/**
 * Parse BootParam CBOR object and extract relevant data.
 */
static int parse_boot_params(const void *blob, size_t size, void *dest_buffer, size_t dest_size,
			     struct pvmfw_boot_params *params)
{
	const char *field;
	struct pvmfw_boot_params_cbor_v1 *v1 = (void *)blob;

	memset(params, 0, sizeof(*params));

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
	 */
	params->handover_size = size - sizeof(*v1);

	/*
	 * Align the entries offsets to 8 bytes to make sure we won't run into
	 * unaligned memory access later.
	 */
	params->handover_offset =
		ALIGN_UP((uintptr_t)dest_buffer, ANDROID_PVMFW_CFG_BLOB_ALIGN) -
		(uintptr_t)dest_buffer;

	/* Make sure that there is enough space for handover in the buffer */
	if (params->handover_offset + params->handover_size > dest_size) {
		printf("Not enough space in the destination buffer for handover data.\n");
		return -1;
	}

	/* Copy handover data into place. */
	memcpy(dest_buffer + params->handover_offset, v1->android_dice_handover,
	       params->handover_size);

	/*
	 * Align the entry offsets to pvmfw config alignment bytes to make
	 * sure we won't run into unaligned memory access later.
	 */
	params->reserved_mem_offset = ALIGN_UP(params->handover_offset + params->handover_size,
					       ANDROID_PVMFW_CFG_BLOB_ALIGN);

	/* The size of the entire entry with headers and blobs */
	params->reserved_mem_size = sizeof(struct pvmfw_cfg_rmem);

	/* Make sure that there is enough space for reserved memory in the buffer */
	if (params->reserved_mem_offset + params->reserved_mem_size > dest_size) {
		printf("Not enough space in the destination buffer for reserved memory.\n");
		return -1;
	}

	copy_reserved_mem(v1,
			  (struct pvmfw_cfg_rmem *)(dest_buffer + params->reserved_mem_offset));

	return 0;
mismatch:
	printf("Failed to parse BootParams CBOR structure. Constant fragment "
	       "around '%s' mismatches expected structure.\n", field);
	return 1;
}

int setup_android_pvmfw(void *buffer, size_t buffer_size, size_t *pvmfw_size,
			const void *params, size_t params_size)
{
	int ret;
	uintptr_t pvmfw_addr, pvmfw_max, cfg_addr;
	struct pvmfw_config_v1_3 *cfg;
	struct pvmfw_boot_params boot_params;

	/* Set the boundaries of the pvmfw buffer */
	pvmfw_addr = (uintptr_t)buffer;
	pvmfw_max = pvmfw_addr + buffer_size;

	/* Align to where pvmfw will search for the config header */
	cfg_addr = ALIGN_UP(pvmfw_addr + *pvmfw_size, ANDROID_PVMFW_CFG_ALIGN);
	cfg = (void *)cfg_addr;

	/* Make sure we have space for config and for some blobs */
	if ((uintptr_t)cfg->blobs >= pvmfw_max) {
		printf("No space in the buffer for pvmfw config\n");
		goto fail;
	}

	/* Parse and extract data from BootParam CBOR object */
	ret = parse_boot_params(params, params_size, cfg->blobs,
				(size_t)(pvmfw_max - (uintptr_t)cfg->blobs),
				&boot_params);
	if (ret != 0) {
		printf("Failed to parse pvmfw gsc boot params data\n");
		goto fail;
	}

	/* Fill the header */
	memset(cfg, 0, sizeof(*cfg));
	memcpy(cfg->magic, ANDROID_PVMFW_CFG_MAGIC, sizeof(cfg->magic));
	cfg->version = ANDROID_PVMFW_CFG_V1_3;

	/*
	 * Fill the entry 0 and entry 4 location. The remaining entries are
	 * not used so leave them as zeros.
	 */
	cfg->dice_handover.offset = offsetof(struct pvmfw_config_v1_3, blobs)
		+ boot_params.handover_offset;
	cfg->dice_handover.size = boot_params.handover_size;

	cfg->reserved_mem.offset =
		offsetof(struct pvmfw_config_v1_3, blobs) + boot_params.reserved_mem_offset;
	cfg->reserved_mem.size = boot_params.reserved_mem_size;

	/* Calculate the total size of the config, using the last used entry */
	cfg->total_size = cfg->reserved_mem.offset + boot_params.reserved_mem_size;

	/*
	 * Truncate the pvmfw's avb footer to minimize reserved memory size
	 * aligned up to config alignment.
	 */
	*pvmfw_size = ALIGN_UP(cfg_addr - pvmfw_addr + cfg->total_size,
			       ANDROID_PVMFW_CFG_ALIGN);

	return 0;
fail:
	*pvmfw_size = 0;
	return 0;
}

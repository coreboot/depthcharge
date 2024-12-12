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
};

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
};

/* pvmfw's entry 4: Reserved memory blob header */
struct pvmfw_cfg_rmem_hdr {
	uint32_t vm_name_offset;
	uint32_t blob_offset;
	uint32_t blob_size;
	uint32_t compat_offset;
	uint32_t flags;
};

#define PVMFW_CFG_RESERVED_MEM_NO_MAP 0x1

#define GSC_EARLY_ENTROPY_SIZE 64
#define GSC_SESSION_KEY_SEED_SIZE 32
#define GSC_AUTH_TOKEN_SIZE 32

/* The value of com.android.virt.name property in Trusty image's AVB vbmeta */
static const char desktop_trusty_name[] = "desktop-trusty";
/* Reserved memory blobs VM DTB compatible strings */
static const char entropy_compat_str[] = "google,early-entropy";
static const char session_compat_str[] = "google,session-key-seed";
static const char auth_token_compat_str[] = "google,auth-token-key-seed";

/* pvmfw's entry 4: Reserved memory blobs with headers */
struct pvmfw_cfg_rmem {
	/* The number of reserved blobs in the entry */
	uint32_t count;

#define PVMFW_CFG_RESERVED_MEM_BLOB_COUNT 3
	/*
	 * The headers for each reserved blob in the entry, all store the
	 * offset and size of blob in the entry.
	 */
	struct {
		struct pvmfw_cfg_rmem_hdr entropy;
		struct pvmfw_cfg_rmem_hdr session;
		struct pvmfw_cfg_rmem_hdr auth;
	} hdrs;

	/*
	 * The payload of reserved blobs entry. The offsets and sizes of
	 * the fields are stored in pvmfw_cfg_rmem_hdr headers above in runtime.
	 */
	struct reserved_blobs {
		uint8_t early_entropy[GSC_EARLY_ENTROPY_SIZE];
		uint8_t session_key_seed[GSC_SESSION_KEY_SEED_SIZE];
		uint8_t auth_token_key_seed[GSC_AUTH_TOKEN_SIZE];

		/*
		 * It is required that string table is at the end of reserved
		 * blobs entry.
		 */
		uint8_t desktop_trusty_name[sizeof(desktop_trusty_name)];
		uint8_t entropy_compat[sizeof(entropy_compat_str)];
		uint8_t session_compat[sizeof(session_compat_str)];
		uint8_t auth_token_compat[sizeof(auth_token_compat_str)];
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
		 * 0x03 - uint = 3	AuthTokenKeySeed label
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

static void copy_reserved_mem(struct pvmfw_boot_params_cbor_v1 *v1,
			      struct pvmfw_cfg_rmem *reserved)
{
	struct reserved_blobs *blobs = &reserved->blobs;

	/* Clear the size of entry that will be used */
	memset(reserved, 0, sizeof(struct pvmfw_cfg_rmem));

	/* Set the count of the reserved memory entiries */
	reserved->count = PVMFW_CFG_RESERVED_MEM_BLOB_COUNT;

	/* Copy VM name to reserved memory blobs string table */
	memcpy(blobs->desktop_trusty_name, desktop_trusty_name, sizeof(desktop_trusty_name));

	/* Setup the reserved memory header for early entropy */
	reserved->hdrs.entropy = (struct pvmfw_cfg_rmem_hdr){
		.vm_name_offset = offsetof(struct reserved_blobs, desktop_trusty_name),
		.blob_offset = offsetof(struct reserved_blobs, early_entropy),
		.blob_size = GSC_EARLY_ENTROPY_SIZE,
		.compat_offset = offsetof(struct reserved_blobs, entropy_compat),
		.flags = PVMFW_CFG_RESERVED_MEM_NO_MAP,
	};

	/* Copy early entropy received from GSC to reserved memory blobs */
	memcpy(blobs->early_entropy, v1->boot_params.early_entropy, GSC_EARLY_ENTROPY_SIZE);
	memcpy(blobs->entropy_compat, entropy_compat_str, sizeof(entropy_compat_str));

	/* Setup the reserved memory header for session key seed */
	reserved->hdrs.session = (struct pvmfw_cfg_rmem_hdr){
		.vm_name_offset = offsetof(struct reserved_blobs, desktop_trusty_name),
		.blob_offset = offsetof(struct reserved_blobs, session_key_seed),
		.blob_size = GSC_SESSION_KEY_SEED_SIZE,
		.compat_offset = offsetof(struct reserved_blobs, session_compat),
		.flags = PVMFW_CFG_RESERVED_MEM_NO_MAP,
	};

	/* Copy session key seed received from GSC to reserved memory blobs */
	memcpy(blobs->session_key_seed, v1->boot_params.session_key_seed,
	       GSC_SESSION_KEY_SEED_SIZE);
	memcpy(blobs->session_compat, session_compat_str, sizeof(session_compat_str));

	/* Setup the reserved memory header for auth token key seed */
	reserved->hdrs.auth = (struct pvmfw_cfg_rmem_hdr){
		.vm_name_offset = offsetof(struct reserved_blobs, desktop_trusty_name),
		.blob_offset = offsetof(struct reserved_blobs, auth_token_key_seed),
		.blob_size = GSC_AUTH_TOKEN_SIZE,
		.compat_offset = offsetof(struct reserved_blobs, auth_token_compat),
		.flags = PVMFW_CFG_RESERVED_MEM_NO_MAP,
	};

	/* Copy auth token key seed received from GSC to first reserved memory */
	memcpy(blobs->auth_token_key_seed, v1->boot_params.auth_token_key_seed,
	       GSC_AUTH_TOKEN_SIZE);
	memcpy(blobs->auth_token_compat, auth_token_compat_str, sizeof(auth_token_compat_str));
}

/**
 * Parse BootParam CBOR object and extract relevant data.
 */
static int parse_boot_params(const void *blob, size_t size, struct pvmfw_config_v1_3 *cfg,
			     size_t max_blobs_size)
{
	size_t offset_blobs;
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
	 * Fill the entry 0 and entry 4 location. The remaining entries are
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

	/*
	 * Align the entry offsets to pvmfw config alignment bytes to make
	 * sure we won't run into unaligned memory access later.
	 * The size of the entire entry contains both headers and blobs.
	 */
	offset_blobs = ALIGN_UP(cfg->dice_handover.size, ANDROID_PVMFW_CFG_BLOB_ALIGN);
	cfg->reserved_mem.offset = offsetof(struct pvmfw_config_v1_3, blobs) + offset_blobs;
	cfg->reserved_mem.size = sizeof(struct pvmfw_cfg_rmem);

	/* Make sure that there is enough space for reserved memory in the buffer */
	if (offset_blobs + cfg->reserved_mem.size > max_blobs_size) {
		printf("Not enough space in the destination buffer for reserved memory.\n");
		return -1;
	}

	/* Copy headers and blobs data into place. */
	copy_reserved_mem(v1, (struct pvmfw_cfg_rmem *)&cfg->blobs[offset_blobs]);

	/* Calculate the total size of the config with blobs, using the last used entry */
	cfg->total_size = cfg->reserved_mem.offset + cfg->reserved_mem.size;

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
	struct pvmfw_config_v1_3 *cfg;

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

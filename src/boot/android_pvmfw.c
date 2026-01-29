// SPDX-License-Identifier: GPL-2.0

#include <commonlib/helpers.h>
#include <endian.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <vb2_api.h>

#include "base/device_tree.h"
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

/*
 * Empty raw DTB. Used for creation of DTB for entry 3.
 * Generated using: `echo "/dts-v1/; /{};" | dtc -I dts -O dtb | xxd  -i`
 */
static const uint8_t empty_dtb[] = {
	0xd0, 0x0d, 0xfe, 0xed, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x38,
	0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x11,
	0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x09,
};

/* pvmfw's entry 4: Reserved memory blobs with headers */
struct pvmfw_cfg_rmem_v0 {
	/* The number of reserved blobs in the entry */
	uint32_t count;

#define PVMFW_CFG_RESERVED_MEM_V1_BLOB_COUNT 3
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

#define PVMFW_BOOT_PARAMS_PREAMBLE_SIZE 3

/**
 * BootParam CBOR object flatten into raw C structure.
 */
struct pvmfw_boot_params_cbor_v0 {
	/*
	 * 0xA3 - CBOR Map 3 elements
	 * 0x01 - uint = 1		Version label
	 * 0x00 - uint = 0		Version
	 */
	uint8_t cbor_labels_preamble[PVMFW_BOOT_PARAMS_PREAMBLE_SIZE];

	/*
	 * 0x02 - uint = 2		Boot parameters label
	 */
	uint8_t cbor_label_boot_params;
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

/* Allocates a space in pvmfw configuration for the given entry */
static int alloc_cfg_entry(struct pvmfw_config_v1_3 *cfg, size_t max_cfg_size,
			   struct pvmfw_config_entry *cfg_entry, size_t size)
{
	/* Place new entry on aligned offset */
	cfg_entry->offset = ALIGN_UP(cfg->total_size, ANDROID_PVMFW_CFG_BLOB_ALIGN);
	cfg_entry->size = size;

	/* Check if destination buffer is big enough */
	if (cfg_entry->offset + cfg_entry->size > max_cfg_size)
		return PVMFW_ERR_CFG_BUFFER_TOO_SMALL;

	/* Update the end of cfg, to the end of appending to blobs */
	cfg->total_size = cfg_entry->offset + cfg_entry->size;

	return PVMFW_SUCCESS;
}

static int create_fw_android_node(struct device_tree *ref_dtb, char *bootconfig)
{
	static const char *const dt_entries[][2] = {
		{"androidboot.vbmeta.digest=", "host.vbmeta.digest"},
		{"androidboot.verifiedbootstate=", "host.verifiedbootstate"},
		{"androidboot.vbmeta.device_state=", "host.vbmeta.device_state"},
		{"androidboot.vbmeta.public_key_digest=", "host.vbmeta.public_key_digest"},
	};
	static const char *const fw_android_path[] = {"firmware", "android", NULL};
	struct device_tree_node *node;
	char *saveptr;
	char *pair;
	size_t len;
	int i;

	/* Create /firmware/android DT node */
	node = dt_find_node(ref_dtb->root, fw_android_path, NULL, NULL, 1);
	if (!node)
		return PVMFW_ERR_DT_CREATE_FAIL;

	/* Set node's compatible string */
	dt_add_string_prop(node, "compatible", "android,firmware");

	/*
	 * Create a copy of the bootconfig so we can use strtok_r safely,
	 * without corrupting state.
	 */
	bootconfig = strdup(bootconfig);
	if (!bootconfig)
		return PVMFW_ERR_NO_MEM;

	/* Iterate over key=value pairs delimited by spaces. */
	for (pair = strtok_r(bootconfig, " ", &saveptr); pair != NULL;
	     pair = strtok_r(NULL, " ", &saveptr)) {
		/* Find the index of key in the dt_entries */
		for (i = 0; i < ARRAY_SIZE(dt_entries); i++) {
			len = strlen(dt_entries[i][0]);

			/* Skip unrelevant pair */
			if (strncmp(pair, dt_entries[i][0], len) != 0)
				continue;

			/* Add just the pair's value with translated property name */
			dt_add_string_prop(node, dt_entries[i][1], pair + len);
			break;
		}
	}

	/* Free the bootconfig copy */
	free(bootconfig);

	return PVMFW_SUCCESS;
}

static int create_avf_vm_ref_dt(const struct vb2_kernel_params *kparams,
				struct device_tree **tree)
{
	struct device_tree *ref_dtb;
	int ret;

	/* Create new empty device tree for the VM reference DT */
	ref_dtb = fdt_unflatten(empty_dtb);
	if (!ref_dtb)
		return PVMFW_ERR_DT_CREATE_FAIL;

	/* Create firmware,android node with data parsed from bootconfig */
	ret = create_fw_android_node(ref_dtb, kparams->bootconfig_cmdline_buffer);
	if (ret)
		return ret;

	*tree = ref_dtb;
	return PVMFW_SUCCESS;
}

static int add_ref_dtb_entry(struct pvmfw_config_v1_3 *cfg, size_t max_cfg_size,
			     struct device_tree *vm_ref_dt)
{
	int ret;

	/* If VM reference DT return with warning */
	if (!vm_ref_dt) {
		printf("WARN: Missing VM reference DT. Ignoring...");
		return PVMFW_SUCCESS;
	}

	/* Allocate space for the VM ref DTB configuration entry */
	ret = alloc_cfg_entry(cfg, max_cfg_size, &cfg->ref_dtb, dt_flat_size(vm_ref_dt));
	if (ret)
		return ret;

	/* Flatten DT structure as FDT to configuration entry */
	dt_flatten(vm_ref_dt, ((uint8_t *)cfg) + cfg->ref_dtb.offset);

	return PVMFW_SUCCESS;
}

static void copy_reserved_mem_v0(struct pvmfw_boot_params_cbor_v0 *v0,
				 struct pvmfw_cfg_rmem_v0 *reserved)
{
	struct reserved_blobs *blobs = &reserved->blobs;

	/* Clear the size of entry that will be used */
	memset(reserved, 0, sizeof(struct pvmfw_cfg_rmem_v0));

	/* Set the count of the reserved memory entiries */
	reserved->count = PVMFW_CFG_RESERVED_MEM_V1_BLOB_COUNT;

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
	memcpy(blobs->early_entropy, v0->boot_params.early_entropy, GSC_EARLY_ENTROPY_SIZE);
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
	memcpy(blobs->session_key_seed, v0->boot_params.session_key_seed,
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
	memcpy(blobs->auth_token_key_seed, v0->boot_params.auth_token_key_seed,
	       GSC_AUTH_TOKEN_SIZE);
	memcpy(blobs->auth_token_compat, auth_token_compat_str, sizeof(auth_token_compat_str));
}

/**
 * Parses the v0 CBOR BootParam = {
 *   1 : 0,		;  version
 *   2 : GSCBootParam,
 *   3 : AndroidDiceHandover
 * }
 *
 * GSCBootParam = {
 *   1  : bstr .size 64, ; EarlyEntropy
 *   2  : bstr .size 32, ; SessionKeySeed
 *   3  : bstr .size 32, ; AuthTokenKeySeed
 * }
 */
static int parse_boot_params_v0(const void *blob, size_t size, struct device_tree *vm_ref_dt,
				struct pvmfw_config_v1_3 *cfg, size_t max_cfg_size)
{
	int ret;
	const char *field;
	struct pvmfw_boot_params_cbor_v0 *v0 = (void *)blob;

	/*
	 * Verify that the blob is at least big enough to compare the
	 * constant fragments.
	 */
	if (size < sizeof(struct pvmfw_boot_params_cbor_v0)) {
		printf("Received BootParams CBOR is too short.\n");
		return PVMFW_ERR_CBOR_TOO_SMALL;
	}

	/*
	 * Verify that the known constant CBOR fragment match the expected
	 * values. The following constant byte strings are described in
	 * pvmfw_boot_params_cbor_v0 comments.
	 */
	static const uint8_t cbor_label_boot_params = 0x02;
	if (v0->cbor_label_boot_params != cbor_label_boot_params) {
		field = "boot_params";
		goto mismatch;
	}

	static const uint8_t cbor_labels_entropy[] = {0xA3, 0x01, 0x58, 0x40};
	if (memcmp(v0->boot_params.cbor_labels_entropy, cbor_labels_entropy,
		   sizeof(cbor_labels_entropy))) {
		field = "early_entropy";
		goto mismatch;
	}

	static const uint8_t cbor_labels_session[] = {0x02, 0x58, 0x20};
	if (memcmp(v0->boot_params.cbor_labels_session, cbor_labels_session,
		   sizeof(cbor_labels_session))) {
		field = "session_key_seed";
		goto mismatch;
	}

	static const uint8_t cbor_labels_auth[] = {0x03, 0x58, 0x20};
	if (memcmp(v0->boot_params.cbor_labels_auth, cbor_labels_auth,
		   sizeof(cbor_labels_auth))) {
		field = "auth_token_key_seed";
		goto mismatch;
	}

	if (v0->cbor_label_handover[0] != 0x03) {
		field = "android_dice_handover";
		goto mismatch;
	}

	/*
	 * Verified that the blob matches the expected structure.
	 *
	 * The remaining bytes of BootParams structure are AndroidDiceHandover
	 * bytes.
	 *
	 * Set the end of configuration to beginning of blobs.
	 */
	cfg->total_size = offsetof(struct pvmfw_config_v1_3, blobs);

	/* Allocate space for handover */
	ret = alloc_cfg_entry(cfg, max_cfg_size, &cfg->dice_handover,
			      size - sizeof(struct pvmfw_boot_params_cbor_v0));
	if (ret)
		return ret;

	/* Copy handover data into place. */
	memcpy(&cfg->blobs[0], v0->android_dice_handover, cfg->dice_handover.size);

	/* Add entry 3: VM reference DT entry and content */
	ret = add_ref_dtb_entry(cfg, max_cfg_size, vm_ref_dt);
	if (ret) {
		printf("Failed to add ref dtb entry: %d\n", ret);
		return ret;
	}

	/*
	 * Allocate space for reserved memory. The size of the entire entry
	 * contains both headers and blobs.
	 */
	ret = alloc_cfg_entry(cfg, max_cfg_size, &cfg->reserved_mem,
			      sizeof(struct pvmfw_cfg_rmem_v0));
	if (ret)
		return ret;

	/* Copy headers and blobs data into place. */
	copy_reserved_mem_v0(v0, (void *)((uintptr_t *)cfg + cfg->reserved_mem.offset));

	return PVMFW_SUCCESS;
mismatch:
	printf("Failed to parse BootParams CBOR structure. Constant fragment "
	       "around '%s' mismatches expected structure.\n",
	       field);
	return PVMFW_ERR_CBOR_MISMATCH;
}

static ssize_t copy_bstr16_to_entry(const void *cbor, size_t cbor_size, size_t cbor_offset,
				    uint8_t cbor_key, struct pvmfw_config_v1_3 *cfg,
				    size_t max_cfg_size, struct pvmfw_config_entry *cfg_entry)
{
	const struct cbor_map_entry {
		/* 0x0X      - uint = X ; Entry key  */
		uint8_t key;
		/* 0x58      - bstr label */
		uint8_t cbor_bstr_label;
		/* 0xXX 0xXX - .size be16 ; Entry size */
		uint16_t size;
		uint8_t data[];
	} *cbor_entry;
	size_t size;
	int ret;

	_Static_assert(sizeof(struct cbor_map_entry) == 4, "CBOR entry bstr 16 bytes size");

	/* Check if entry labels are in within range */
	if (cbor_offset + sizeof(*cbor_entry) > cbor_size)
		return PVMFW_ERR_CBOR_TOO_SMALL;

	cbor_entry = cbor + cbor_offset;

	/* Check if entry key is correct */
	if (cbor_entry->key != cbor_key)
		return PVMFW_ERR_CBOR_MISMATCH;

	/* Check if bstr label is correct */
	static const uint8_t cbor_bstr16_label = 0x59;
	if (cbor_entry->cbor_bstr_label != cbor_bstr16_label)
		return PVMFW_ERR_CBOR_MISMATCH;

	/* bstr 16-bit size stored in big endian */
	size = be16toh(cbor_entry->size);

	/* Check if bstr contents are within range */
	if (cbor_offset + sizeof(*cbor_entry) + size > cbor_size)
		return PVMFW_ERR_CBOR_TOO_SMALL;

	ret = alloc_cfg_entry(cfg, max_cfg_size, cfg_entry, size);
	if (ret)
		return ret;

	/* Copy the actual payload */
	memcpy(((void *)cfg) + cfg_entry->offset, cbor_entry->data, size);

	/* Return consumed CBOR byte count */
	return sizeof(*cbor_entry) + size;
}

/**
 * Parses the v1 CBOR BootParam = {
 *   1 : 1,         ; version
 *   4 : bstr16 .cbor AndroidDiceHandover,
 *   5 : bstr16,    ; entry 4, VM reserved memory
 * }
 */
static int parse_boot_params_v1(const void *cbor, size_t cbor_size,
				struct device_tree *vm_ref_dt, struct pvmfw_config_v1_3 *cfg,
				size_t max_cfg_size)
{
	int ret;
	ssize_t consumed;
	const char *field;
	size_t cbor_rmem_offset;

	/* Set the end of cfg to beginning of blobs */
	cfg->total_size = offsetof(struct pvmfw_config_v1_3, blobs);

	/* 4 : bstr16 .cbor AndroidDiceHandover */
	static const uint8_t cbor_dice_key = 0x04;
	static const uint8_t cbor_dice_offset = PVMFW_BOOT_PARAMS_PREAMBLE_SIZE;
	consumed = copy_bstr16_to_entry(cbor, cbor_size, cbor_dice_offset, cbor_dice_key, cfg,
					max_cfg_size, &cfg->dice_handover);
	if (consumed < 0) {
		field = "android_dice_handover_v1";
		goto err;
	}

	/* Add entry 3: VM reference DT entry and content */
	ret = add_ref_dtb_entry(cfg, max_cfg_size, vm_ref_dt);
	if (ret) {
		field = "ref_dtb";
		consumed = ret;
		goto err;
	}

	/* 5 : bstr16 ; entry 4, VM reserved memory */
	static const uint8_t cbor_rmem_key = 0x05;
	cbor_rmem_offset = cbor_dice_offset + consumed;
	consumed = copy_bstr16_to_entry(cbor, cbor_size, cbor_rmem_offset, cbor_rmem_key, cfg,
					max_cfg_size, &cfg->reserved_mem);
	if (consumed < 0) {
		field = "reserved_memory";
		goto err;
	}

	return PVMFW_SUCCESS;
err:
	printf("Failed to copy '%s' from CBOR. (consumed: %zd)\n", field, consumed);
	return consumed;
}

/**
 * Parse BootParam CBOR object and extract relevant data.
 */
static int parse_boot_params(const void *blob, size_t size, struct device_tree *vm_ref_dt,
			     struct pvmfw_config_v1_3 *cfg, size_t max_cfg_size)
{
	/*
	 * Verify that the blob is at least big enough to compare the
	 * constant fragments.
	 */
	if (size < PVMFW_BOOT_PARAMS_PREAMBLE_SIZE) {
		printf("Received BootParams CBOR is too short.\n");
		return PVMFW_ERR_CBOR_TOO_SMALL;
	}

	static const uint8_t cbor_labels_preamble_v1[] = {0xA3, 0x01, 0x01};
	if (!memcmp(blob, cbor_labels_preamble_v1, sizeof(cbor_labels_preamble_v1)))
		return parse_boot_params_v1(blob, size, vm_ref_dt, cfg, max_cfg_size);


	static const uint8_t cbor_labels_preamble_v0[] = {0xA3, 0x01, 0x00};
	if (!memcmp(blob, cbor_labels_preamble_v0, sizeof(cbor_labels_preamble_v0)))
		return parse_boot_params_v0(blob, size, vm_ref_dt, cfg, max_cfg_size);

	printf("Received BootParams CBOR version not recognized. "
	       "GSC firmware version is much newer then AP firmware?\n");
	return PVMFW_ERR_CBOR_MISMATCH;
}

int setup_android_pvmfw(const struct vb2_kernel_params *kparams, size_t *pvmfw_size,
			const void *params, size_t params_size)
{
	int ret;
	uintptr_t pvmfw_addr, pvmfw_max, cfg_addr;
	struct pvmfw_config_v1_3 *cfg;
	struct device_tree *vm_ref_dt;

	/* Create the unflatten VM reference DT */
	ret = create_avf_vm_ref_dt(kparams, &vm_ref_dt);
	if (ret) {
		printf("Failed to create VM ref DT (error: %d). Ignoring...\n", ret);
		vm_ref_dt = NULL;
	}

	/* Set the boundaries of the pvmfw buffer */
	pvmfw_addr = (uintptr_t)kparams->pvmfw_buffer;
	pvmfw_max = pvmfw_addr + kparams->pvmfw_buffer_size;

	/* Align to where pvmfw will search for the config header */
	cfg_addr = ALIGN_UP(pvmfw_addr + kparams->pvmfw_out_size, ANDROID_PVMFW_CFG_ALIGN);
	cfg = (void *)cfg_addr;

	/* Make sure we have space for config and for some blobs */
	if ((uintptr_t)cfg->blobs >= pvmfw_max) {
		printf("No space in the buffer for pvmfw config\n");
		ret = PVMFW_ERR_CFG_BUFFER_TOO_SMALL;
		goto fail;
	}

	/* Fill the header */
	memset(cfg, 0, sizeof(*cfg));
	memcpy(cfg->magic, ANDROID_PVMFW_CFG_MAGIC, sizeof(cfg->magic));
	cfg->version = ANDROID_PVMFW_CFG_V1_3;

	/* Parse and extract data from BootParam CBOR object */
	ret = parse_boot_params(params, params_size, vm_ref_dt, cfg,
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

	return PVMFW_SUCCESS;
fail:
	*pvmfw_size = 0;
	return ret;
}

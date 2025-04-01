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
	cfg->version = ANDROID_PVMFW_CFG_V1_2;

	/* Set the config size to the beginning of the blobs */
	cfg->total_size = offsetof(struct pvmfw_config_v1_2, blobs);

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

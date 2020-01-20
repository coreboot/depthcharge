/*
 * Copyright 2012 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define NEED_VB20_INTERNALS  /* Peeking into vb2_shared_data */

#include <assert.h>
#include <libpayload.h>
#include <vb2_api.h>
#include <vboot_struct.h>

#include "drivers/flash/flash.h"
#include "image/fmap.h"
#include "image/symbols.h"
#include "vboot/stages.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"

static char shared_data[VB_SHARED_DATA_MIN_SIZE];

int gbb_clear_flags(void)
{
	if (flash_is_wp_enabled()) {
		printf("WP is enabled; can't write to GBB.\n");
		return 1;
	}

	FmapArea area;
	if (fmap_find_area("GBB", &area)) {
		printf("Couldn't find GBB area.\n");
		return 1;
	}

	vb2_gbb_flags_t new_flags = 0;
	if (sizeof(new_flags) !=
	    flash_rewrite(area.offset + VB2_GBB_FLAGS_OFFSET,
			  sizeof(new_flags), &new_flags)) {
		printf("Write to GBB failed.\n");
		return 1;
	}

	return 0;
}

int vboot_create_vbsd(void)
{
	VbSharedDataHeader *vb_sd = (VbSharedDataHeader *)shared_data;
	struct vb2_shared_data *vb2_sd;

	/* Get reference to vb2_shared_data struct. */
	if (lib_sysinfo.vboot_workbuf == NULL) {
		printf("%s: vboot working data pointer is NULL\n", __func__);
		return 1;
	}
	vb2_sd = lib_sysinfo.vboot_workbuf;
	if (vb2_sd->magic != VB2_SHARED_DATA_MAGIC) {
		printf("%s: vb2_shared_data has invalid magic\n", __func__);
		return 1;
	}

	printf("%s: creating legacy VbSharedDataHeader structure\n", __func__);
	memset(&shared_data, 0, sizeof(shared_data));

	vb_sd->flags |= VBSD_BOOT_FIRMWARE_VBOOT2;
	vb_sd->firmware_index = vb2_sd->fw_slot;
	vb_sd->magic = VB_SHARED_DATA_MAGIC;
	vb_sd->struct_version = VB_SHARED_DATA_VERSION;
	vb_sd->struct_size = sizeof(VbSharedDataHeader);
	vb_sd->data_size = VB_SHARED_DATA_MIN_SIZE;
	vb_sd->data_used = sizeof(VbSharedDataHeader);
	vb_sd->fw_version_tpm = vb2_sd->fw_version_secdata;

	if (vb2_sd->flags & VB2_SD_FLAG_DEV_MODE_ENABLED) {
		vb_sd->flags |= VBSD_BOOT_DEV_SWITCH_ON;
		vb_sd->flags |= VBSD_LF_DEV_SWITCH_ON;
	}

	/* copy kernel subkey if it's found */
	if (vb2_sd->preamble_size) {
		struct vb2_fw_preamble *fp;
		uintptr_t dst, src;
		printf("%s: copying FW preamble\n",  __func__);
		fp = (struct vb2_fw_preamble *)((uintptr_t)vb2_sd +
				vb2_sd->preamble_offset);
		src = (uintptr_t)&fp->kernel_subkey +
				fp->kernel_subkey.key_offset;
		dst = (uintptr_t)vb_sd + sizeof(VbSharedDataHeader);
		if (dst + fp->kernel_subkey.key_size >
		    (uintptr_t)shared_data + sizeof(shared_data)) {
			printf("%s: kernel subkey size too large\n", __func__);
			return 1;
		}
		memcpy((void *)dst, (void *)src,
		       fp->kernel_subkey.key_size);
		vb_sd->data_used += fp->kernel_subkey.key_size;
		vb_sd->kernel_subkey.key_offset =
				dst - (uintptr_t)&vb_sd->kernel_subkey;
		vb_sd->kernel_subkey.key_size = fp->kernel_subkey.key_size;
		vb_sd->kernel_subkey.algorithm = fp->kernel_subkey.algorithm;
		vb_sd->kernel_subkey.key_version =
				fp->kernel_subkey.key_version;
	}

	/*
	 * If the lid is closed, kernel selection should not count down the
	 * boot tries for updates, since the OS will shut down before it can
	 * register success.
	 */
	if (!flag_fetch(FLAG_LIDSW))
		vb_sd->flags |= VBSD_NOFAIL_BOOT;

	if (flag_fetch(FLAG_WPSW))
		vb_sd->flags |= VBSD_BOOT_FIRMWARE_WP_ENABLED;

	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		vb_sd->flags |= VBSD_BOOT_REC_SWITCH_VIRTUAL;

	return 0;
}

int find_common_params(void **blob, int *size)
{
	*blob = &shared_data[0];
	*size = ARRAY_SIZE(shared_data);
	return 0;
}

struct vb2_context *vboot_get_context(void)
{
	static struct vb2_context *ctx;
	vb2_error_t rv;

	if (ctx)
		return ctx;

	die_if(lib_sysinfo.vboot_workbuf == NULL,
	       "vboot workbuf pointer is NULL\n");

	/* Use the firmware verification workbuf from coreboot. */
	rv = vb2api_reinit(lib_sysinfo.vboot_workbuf, &ctx);

	die_if(rv, "vboot workbuf could not be initialized, error: %#x\n", rv);

	return ctx;
}

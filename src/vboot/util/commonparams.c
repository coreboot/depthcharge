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

static struct vboot_handoff handoff_data;

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

static int vboot_update_shared_data(void)
{
	VbSharedDataHeader *vb_sd;
	int vb_sd_size;

	if (find_common_params((void **)&vb_sd, &vb_sd_size)) {
		printf("Unable to access VBSD\n");
		return 1;
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

	if (CONFIG(EC_SOFTWARE_SYNC))
		vb_sd->flags |= VBSD_EC_SOFTWARE_SYNC;

	if (CONFIG(EC_SLOW_UPDATE))
		vb_sd->flags |= VBSD_EC_SLOW_UPDATE;

	if (CONFIG(EC_EFS))
		vb_sd->flags |= VBSD_EC_EFS;

	if (!CONFIG(PHYSICAL_REC_SWITCH))
		vb_sd->flags |= VBSD_BOOT_REC_SWITCH_VIRTUAL;

	return 0;
}

static int vboot_update_vbinit_flags(void)
{
	/* VbInit was already called in coreboot, so we need to update the
	 * vboot internal flags ourself. */
	return vboot_do_init_out_flags(handoff_data.out_flags);
}

/**
 * Sets vboot_handoff based on the information in vb2_shared_data
 */
static int vboot_fill_handoff(struct vboot_handoff *vboot_handoff,
			      struct vb2_shared_data *vb2_sd)
{
	VbSharedDataHeader *vb_sd =
		(VbSharedDataHeader *)vboot_handoff->shared_data;
	uint32_t *oflags = &vboot_handoff->out_flags;

	vb_sd->flags |= VBSD_BOOT_FIRMWARE_VBOOT2;

	vboot_handoff->selected_firmware = vb2_sd->fw_slot;

	vb_sd->firmware_index = vb2_sd->fw_slot;

	vb_sd->magic = VB_SHARED_DATA_MAGIC;
	vb_sd->struct_version = VB_SHARED_DATA_VERSION;
	vb_sd->struct_size = sizeof(VbSharedDataHeader);
	vb_sd->data_size = VB_SHARED_DATA_MIN_SIZE;
	vb_sd->data_used = sizeof(VbSharedDataHeader);
	vb_sd->fw_version_tpm = vb2_sd->fw_version_secdata;

	if (vb2_sd->recovery_reason) {
		vb_sd->firmware_index = 0xFF;
		if (vb2_sd->flags & VB2_SD_FLAG_MANUAL_RECOVERY)
			vb_sd->flags |= VBSD_BOOT_REC_SWITCH_ON;
		*oflags |= VB_INIT_OUT_ENABLE_RECOVERY;
		*oflags |= VB_INIT_OUT_CLEAR_RAM;
	}
	if (vb2_sd->flags & VB2_SD_FLAG_DEV_MODE_ENABLED) {
		*oflags |= VB_INIT_OUT_ENABLE_DEVELOPER;
		*oflags |= VB_INIT_OUT_CLEAR_RAM;
		vb_sd->flags |= VBSD_BOOT_DEV_SWITCH_ON;
		vb_sd->flags |= VBSD_LF_DEV_SWITCH_ON;
	}

	/* In vboot1, VBSD_FWB_TRIED is
	 * set only if B is booted as explicitly requested. Therefore, if B is
	 * booted because A was found bad, the flag should not be set. It's
	 * better not to touch it if we can only ambiguously control it. */
	/* if (vb2_sd->fw_slot)
		vb_sd->flags |= VBSD_FWB_TRIED; */

	/* copy kernel subkey if it's found */
	if (vb2_sd->workbuf_preamble_size) {
		struct vb2_fw_preamble *fp;
		uintptr_t dst, src;
		printf("vboot_handoff: copying FW preamble\n");
		fp = (struct vb2_fw_preamble *)((uintptr_t)vb2_sd +
				vb2_sd->workbuf_preamble_offset);
		src = (uintptr_t)&fp->kernel_subkey +
				fp->kernel_subkey.key_offset;
		dst = (uintptr_t)vb_sd + sizeof(VbSharedDataHeader);
		if (dst + fp->kernel_subkey.key_size >
		    (uintptr_t)vboot_handoff + sizeof(*vboot_handoff)) {
			printf("vboot_handoff: kernel subkey size too large\n");
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

	vb_sd->recovery_reason = vb2_sd->recovery_reason;

	return 0;
}

static int vboot_create_handoff(void)
{
	struct vb2_shared_data *sd;

	/* Get reference to vb2_shared_data struct. */
	if (lib_sysinfo.vboot_workbuf == NULL) {
		printf("vboot_handoff: vboot working data pointer is NULL\n");
		return 1;
	}
	sd = lib_sysinfo.vboot_workbuf;
	if (sd->magic != VB2_SHARED_DATA_MAGIC) {
		printf("vboot_handoff: vb2_shared_data has invalid magic\n");
		return 1;
	}

	printf("vboot_handoff: creating legacy vboot_handoff structure\n");
	memset(&handoff_data, 0, sizeof(handoff_data));
	return vboot_fill_handoff(&handoff_data, sd);
}

int common_params_init(void)
{
	// Convert incoming vb2_shared_data to vboot_handoff.
	if (vboot_create_handoff())
		return 1;

	// Modify VbSharedDataHeader contents from vboot_handoff struct.
	if (vboot_update_shared_data())
		return 1;

	// Retrieve data from VbInit flags.
	if (vboot_update_vbinit_flags())
		return 1;

	return 0;
}

int find_common_params(void **blob, int *size)
{
	*blob = &handoff_data.shared_data[0];
	*size = ARRAY_SIZE(handoff_data.shared_data);
	return 0;
}

struct vb2_context *vboot_get_context(void)
{
	static struct vb2_context ctx;

	if (ctx.workbuf)
		return &ctx;

	die_if(lib_sysinfo.vboot_workbuf == NULL,
	       "vboot workbuf pointer is NULL\n");

	ctx.workbuf_size = VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE;
	die_if(lib_sysinfo.vboot_workbuf_size > ctx.workbuf_size,
	       "previous workbuf too big\n");

	/* Import the firmware verification workbuf, which includes
	 * vb2_shared_data. */
	ctx.workbuf = xmemalign(VB2_WORKBUF_ALIGN,
				VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE);
	memcpy(ctx.workbuf, lib_sysinfo.vboot_workbuf,
	       lib_sysinfo.vboot_workbuf_size);
	ctx.workbuf_used = lib_sysinfo.vboot_workbuf_size;

	return &ctx;
}

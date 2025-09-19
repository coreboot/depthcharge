/*
 * Copyright 2012 Google LLC
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

#include <image/fmap.h>
#include <libpayload.h>
#include <lzma.h>
#include <vb2_api.h>

#include "base/android_misc.h"
#include "debug/dev.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/flash/flash.h"
#include "fastboot/fastboot.h"
#include "vboot/nvdata.h"
#include "vboot/secdata_tpm.h"

vb2_error_t vb2ex_read_resource(struct vb2_context *ctx,
			enum vb2_resource_index index,
			uint32_t offset,
			void *buf,
			uint32_t size)
{
	FmapArea area;

	if (index != VB2_RES_GBB)
		return VB2_ERROR_EX_READ_RESOURCE_INDEX;

	if (fmap_find_area("GBB", &area)) {
		printf("%s: couldn't find GBB region\n", __func__);
		return VB2_ERROR_EX_READ_RESOURCE_INDEX;
	}

	if ((offset + size) > area.size) {
		printf("%s: offset outside of GBB region\n", __func__);
		return VB2_ERROR_EX_READ_RESOURCE_SIZE;
	}

	if (flash_read(buf, area.offset + offset, size) != size) {
		printf("%s: failed to read from GBB region\n", __func__);
		return VB2_ERROR_EX_READ_RESOURCE_INDEX;
	}

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_commit_data(struct vb2_context *ctx)
{
	vb2_error_t rv = VB2_SUCCESS;
	vb2_error_t nvdata_rv;
	uint32_t tpm_rv;

	/* Write secdata spaces.  Firmware never needs to write secdata_fwmp. */

	tpm_rv = secdata_kernel_write(ctx);
	if (tpm_rv) {
		printf("%s: write secdata_kernel returned %#x\n",
		       __func__, tpm_rv);
		if (!(ctx->flags & VB2_CONTEXT_RECOVERY_MODE)) {
			vb2api_fail(ctx, VB2_RECOVERY_RW_TPM_W_ERROR, rv);
			rv = VB2_ERROR_SECDATA_KERNEL_WRITE;
		}
	}

	tpm_rv = secdata_firmware_write(ctx);
	if (tpm_rv) {
		printf("%s: write secdata_firmware returned %#x\n",
		       __func__, tpm_rv);
		if (!(ctx->flags & VB2_CONTEXT_RECOVERY_MODE)) {
			vb2api_fail(ctx, VB2_RECOVERY_RW_TPM_W_ERROR, rv);
			rv = VB2_ERROR_SECDATA_FIRMWARE_WRITE;
		}
	}

	nvdata_rv = nvdata_write(ctx);
	if (nvdata_rv) {
		printf("%s: write nvdata returned %#x\n",
		       __func__, nvdata_rv);
		/*
		 * We can't write to nvdata, so it's impossible to trigger
		 * recovery mode.  Skip calling vb2api_fail and just die.
		 */
		if (!(ctx->flags & VB2_CONTEXT_RECOVERY_MODE))
			die("can't write recovery reason to nvdata");
		/* If we *are* in recovery mode, ignore any error and return. */
		return VB2_SUCCESS;
	}

	return rv;
}

vb2_error_t vb2ex_handle_android_misc_partition(struct vb2_context *ctx,
						vb2ex_disk_handle_t disk,
						GptData *gpt,
						enum vb2_android_bootmode *bootmode)
{
	enum android_misc_bcb_command cmd;
	BlockDev *bdev = (BlockDev *)disk;

	/* Ignore misc partition on external disks */
	if (bdev->removable) {
		*bootmode = VB2_ANDROID_NORMAL_BOOT;
		return VB2_SUCCESS;
	}

	cmd = android_misc_get_bcb_command(bdev, gpt);
	switch (cmd) {
	case MISC_BCB_NORMAL_BOOT:
	/* For unknown command, just continue with normal boot */
	case MISC_BCB_UNKNOWN:
	/*
	 * If there is problem with misc partition, just allow normal boot and let OS to
	 * figure it out if it boot
	 */
	case MISC_BCB_DISK_ERROR:
		*bootmode = VB2_ANDROID_NORMAL_BOOT;
		break;
	case MISC_BCB_RECOVERY_BOOT:
		*bootmode = VB2_ANDROID_RECOVERY_BOOT;
		break;
	case MISC_BCB_BOOTLOADER_BOOT:
		*bootmode = VB2_ANDROID_NORMAL_BOOT;
		/* If GBB flag is set, start fastboot */
		if (vb2api_gbb_get_flags(ctx) & VB2_GBB_FLAG_FORCE_UNLOCK_FASTBOOT) {
			dc_usb_initialize();
			fastboot();
		}
		break;
	}

	return VB2_SUCCESS;
}

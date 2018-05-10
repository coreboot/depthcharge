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

#include <assert.h>
#include <libpayload.h>
#include <stdint.h>
#include <vboot_api.h>
#include <vboot_nvstorage.h>

#include "base/timestamp.h"
#include "boot/commandline.h"
#include "boot/multiboot.h"
#include "config.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/vboot_ec.h"
#include "drivers/flash/flash.h"
#include "drivers/input/input.h"
#include "drivers/power/power.h"
#include "drivers/storage/blockdev.h"
#include "image/fmap.h"
#include "image/symbols.h"
#include "vboot/boot.h"
#include "vboot/boot_policy.h"
#include "vboot/stages.h"
#include "vboot/crossystem/crossystem.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"
#include "vboot/util/memory.h"
#include "vboot/vbnv.h"

static uint32_t vboot_out_flags;

int vboot_in_recovery(void)
{
	return vboot_out_flags & VB_INIT_OUT_ENABLE_RECOVERY;
}

int vboot_in_developer(void)
{
	return vboot_out_flags & VB_INIT_OUT_ENABLE_DEVELOPER;
}

void vboot_update_recovery(uint32_t request)
{
	vbnv_write(VBNV_RECOVERY_REQUEST, request);
}

int vboot_do_init_out_flags(uint32_t out_flags)
{
	if (out_flags & VB_INIT_OUT_CLEAR_RAM) {
		if (memory_wipe_unused())
			return 1;
	}
	/*
	 * If in developer mode or recovery mode, assume we're going to need
	 * input. We'll want it up and responsive by the time we present
	 * prompts to the user, so get it going ahead of time.
	 */
	if (out_flags & (VB_INIT_OUT_ENABLE_DEVELOPER |
			 VB_INIT_OUT_ENABLE_RECOVERY))
		input_enable();

	vboot_out_flags = out_flags;

	return 0;
}

int vboot_select_and_load_kernel(void)
{
	VbSelectAndLoadKernelParams kparams = {
		.kernel_buffer = &_kernel_start,
		.kernel_buffer_size = &_kernel_end - &_kernel_start,
	};

	if (IS_ENABLED(CONFIG_DETACHABLE_UI)) {
		kparams.inflags = VB_SALK_INFLAGS_ENABLE_DETACHABLE_UI;
		/* On x86 systems, disable power button pulse from EC. */
		cros_ec_config_powerbtn(0);
	}

	printf("Calling VbSelectAndLoadKernel().\n");
	VbError_t res = VbSelectAndLoadKernel(&cparams, &kparams);

	if (IS_ENABLED(CONFIG_DETACHABLE_UI)) {
		/* On x86 systems, enable power button pulse from EC. */
		cros_ec_config_powerbtn(EC_POWER_BUTTON_ENABLE_PULSE);
	}

	if (res == VBERROR_EC_REBOOT_TO_RO_REQUIRED) {
		reboot_all_ecs();
		if (power_off())
			return 1;
	} else if (res == VBERROR_SHUTDOWN_REQUESTED) {
		printf("Powering off.\n");
		if (power_off())
			return 1;
	} else if (res == VBERROR_REBOOT_REQUIRED) {
		printf("Reboot requested. Doing a cold reboot.\n");
		if (cold_reboot())
			return 1;
	}
	if (res != VBERROR_SUCCESS) {
		printf("VbSelectAndLoadKernel returned %d, "
		       "Doing a cold reboot.\n", res);
		if (cold_reboot())
			return 1;
	}

	vboot_boot_kernel(&kparams);

	return 1;
}

void vboot_boot_kernel(VbSelectAndLoadKernelParams *kparams)
{
	static char cmd_line_buf[2 * CmdLineSize];
	struct boot_info bi;

	timestamp_add_now(TS_VB_VBOOT_DONE);

	memset(&bi, 0, sizeof(bi));

	if (fill_boot_info(&bi, kparams) == -1) {
		printf("ERROR!!! Unable to parse boot info\n");
		goto fail;
	}

	bi.kparams = kparams;

	BlockDev *bdev = (BlockDev *)kparams->disk_handle;

	struct commandline_info info = {
		.devnum = 0,
		.partnum = kparams->partition_number + 1,
		.guid = kparams->partition_guid,
		.external_gpt = bdev->external_gpt,
	};

	if (bi.cmd_line) {
		if (commandline_subst(bi.cmd_line, cmd_line_buf,
				      sizeof(cmd_line_buf), &info))
			return;
		bi.cmd_line = cmd_line_buf;
	}

	if (crossystem_setup(FIRMWARE_TYPE_AUTO_DETECT))
		return;

	boot(&bi);

fail:
	/*
	 * If the boot succeeded we'd never end up here. If configured, let's
	 * try booting in alternative way.
	 */
	if (CONFIG_KERNEL_LEGACY)
		legacy_boot(bi.kernel, cmd_line_buf);
	if (CONFIG_KERNEL_MULTIBOOT)
		multiboot_boot(&bi);
}

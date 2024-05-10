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

#include <assert.h>
#include <libpayload.h>
#include <lp_vboot.h>
#include <stdint.h>
#include <vb2_api.h>

#include "arch/post_code.h"
#include "base/cleanup_funcs.h"
#include "base/timestamp.h"
#include "base/vpd_util.h"
#include "boot/commandline.h"
#include "boot/multiboot.h"
#include "drivers/ec/vboot_ec.h"
#include "drivers/flash/flash.h"
#include "drivers/power/power.h"
#include "drivers/storage/blockdev.h"
#include "drivers/bus/usb/usb.h"
#include "image/fmap.h"
#include "image/symbols.h"
#include "vboot/boot.h"
#include "vboot/boot_policy.h"
#include "vboot/crossystem/crossystem.h"
#include "vboot/nvdata.h"
#include "vboot/secdata_tpm.h"
#include "vboot/stages.h"
#include "vboot/util/flag.h"
#include "vboot/util/memory.h"

int vboot_in_recovery(void)
{
	return !!(vboot_get_context()->flags & VB2_CONTEXT_RECOVERY_MODE);
}

int vboot_in_manual_recovery(void)
{
	return vboot_get_context()->boot_mode ==
	       VB2_BOOT_MODE_MANUAL_RECOVERY;
}

int vboot_in_developer(void)
{
	return !!(vboot_get_context()->flags & VB2_CONTEXT_DEVELOPER_MODE);
}

int vboot_check_wipe_memory(void)
{
	if (vboot_get_context()->boot_mode != VB2_BOOT_MODE_NORMAL)
		return memory_wipe_unused();
	return 0;
}

int vboot_check_enable_usb(void)
{
	/* Initialize USB in developer, recovery mode or diagnostics mode,
	   skip in normal mode. */
	switch (vboot_get_context()->boot_mode) {
		case VB2_BOOT_MODE_MANUAL_RECOVERY:
		case VB2_BOOT_MODE_BROKEN_SCREEN:
		case VB2_BOOT_MODE_DIAGNOSTICS:
		case VB2_BOOT_MODE_DEVELOPER:
			dc_usb_initialize();
		default:
			break;
	}
	return 0;
}

static int x86_ec_powerbtn_cleanup_func(struct CleanupFunc *c, CleanupType t)
{
	VbootEcOps *ec = c->data;
	// Reenable power button pulse that we inhibited on x86 systems with UI.
	return ec->enable_power_button(ec, 1);
}
static CleanupFunc x86_ec_powerbtn_cleanup = {
	&x86_ec_powerbtn_cleanup_func,
	CleanupOnReboot | CleanupOnPowerOff |
	CleanupOnHandoff | CleanupOnLegacy,
	NULL,
};

static int commit_and_lock_cleanup_func(struct CleanupFunc *c, CleanupType t)
{
	struct vb2_context *ctx = vboot_get_context();

	if (vb2ex_commit_data(ctx)) {
		if (t != CleanupOnReboot)
			reboot();
		return 0;
	}

	/* No need to lock secdata_kernel on reboot/poweroff. */
	if (t == CleanupOnReboot || t == CleanupOnPowerOff)
		return 0;

	uint32_t tpm_rv = 0;

	if (ctx->flags & VB2_CONTEXT_DISABLE_TPM)
		/* This includes locking secdata_kernel space. */
		tpm_rv = (uint32_t)vb2ex_tpm_set_mode(VB2_TPM_MODE_DISABLED);
	else if (!(ctx->flags & VB2_CONTEXT_RECOVERY_MODE))
		tpm_rv = secdata_kernel_lock(ctx);
	else
		printf("%s: not locking secdata_kernel in recovery mode\n",
		       __func__);

	if (tpm_rv) {
		printf("%s: failed to disable/lock tpm: %#x\n",
		       __func__, tpm_rv);
		vb2api_fail(ctx, VB2_RECOVERY_RW_TPM_L_ERROR, tpm_rv);
		vb2ex_commit_data(ctx);
		reboot();
	}

	return 0;
}

static CleanupFunc commit_and_lock_cleanup = {
	&commit_and_lock_cleanup_func,
	CleanupOnReboot | CleanupOnPowerOff |
	CleanupOnHandoff | CleanupOnLegacy,
	NULL,
};

int vboot_select_and_boot_kernel(void)
{
	struct vb2_context *ctx = vboot_get_context();

	struct vb2_kernel_params kparams = {
		.kernel_buffer = _kernel_start,
		.kernel_buffer_size = _kernel_end - _kernel_start,
	};
	VbootEcOps *ec = vboot_get_ec();

	// On x86 systems, inhibit power button pulse from EC.
	if (CONFIG(ARCH_X86) && ec &&
	    ec->enable_power_button) {
		ec->enable_power_button(ec, 0);
		x86_ec_powerbtn_cleanup.data = ec;
		list_insert_after(&x86_ec_powerbtn_cleanup.list_node,
				  &cleanup_funcs);
	}

	if (CONFIG(EC_VBOOT_SUPPORT))
		ctx->flags |= VB2_CONTEXT_EC_SYNC_SUPPORTED;

	/*
	 * If the lid is closed, kernel selection should not count down the
	 * boot tries for updates, since the OS will shut down before it can
	 * register success.
	 */
	if (!flag_fetch(FLAG_LIDSW))
		ctx->flags |= VB2_CONTEXT_NOFAIL_BOOT;

	/*
	 * Read secdata_fwmp spaces. No need to read secdata firmware or kernel
	 * since it was already read during firmware verification. We don't
	 * check the return value here because vb2api_kernel_phase1 will catch
	 * invalid secdata and tell us what to do (=reboot).
	 */
	secdata_fwmp_read(ctx);

	/* Commit and lock data spaces right before booting a kernel or
	   chainloading to another firmware. */
	list_insert_after(&commit_and_lock_cleanup.list_node,
			  &cleanup_funcs);

	post_code(POST_CODE_KERNEL_PHASE_1);

	vb2_error_t res = vb2api_kernel_phase1(ctx);
	if (res != VB2_SUCCESS)
		goto fail;

	post_code(POST_CODE_KERNEL_PHASE_2);

	res = vb2api_kernel_phase2(ctx);
	if (res != VB2_SUCCESS)
		goto fail;

	post_code(POST_CODE_KERNEL_LOAD);

	res = vboot_select_and_load_kernel(ctx, &kparams);
	if (res != VB2_SUCCESS)
		goto fail;

	if (!vboot_in_recovery()) {
		uint32_t tpm_rv = secdata_extend_kernel_pcr(ctx);
		if (tpm_rv) {
			printf("failed to extend kernel PCR: %#x\n", tpm_rv);
			vb2api_fail(ctx, VB2_RECOVERY_RW_TPM_W_ERROR, tpm_rv);
			reboot();
		}
	}

	if (CONFIG(WIDEVINE_PROVISION) && !vboot_in_recovery()) {
		uint32_t tpm_rv = secdata_widevine_prepare(ctx);
		if (tpm_rv) {
			printf("failed to prepare widevine data: %#x\n",
			       tpm_rv);
			vb2api_fail(ctx, VB2_RECOVERY_WIDEVINE_PREPARE, tpm_rv);
			reboot();
		}
	}

	post_code(POST_CODE_KERNEL_FINALIZE);

	res = vb2api_kernel_finalize(ctx);

fail:
	if (res == VB2_REQUEST_REBOOT_EC_TO_RO) {
		printf("EC Reboot requested. Doing cold reboot.\n");
		if (ec && ec->reboot_to_ro)
			ec->reboot_to_ro(ec);
		power_off();
	} else if (res == VB2_REQUEST_REBOOT_EC_SWITCH_RW) {
		printf("Switch EC slot requested. Doing cold reboot.\n");
		if (ec && ec->reboot_switch_rw)
			ec->reboot_switch_rw(ec);
		power_off();
	} else if (res == VB2_REQUEST_SHUTDOWN) {
		printf("Powering off.\n");
		if (ec && ec->reboot_ap_off && vboot_in_manual_recovery()) {
			/*
			 * In recovery mode, EC will reset itself on shutdown.
			 * So, we need to tell it not to start AP.
			 */
			printf("Powering off (with AP_OFF).\n");
			ec->reboot_ap_off(ec);
		}
		power_off();
	} else if (res == VB2_REQUEST_REBOOT) {
		printf("Reboot requested. Doing warm reboot.\n");
		reboot();
	}
	if (res != VB2_SUCCESS) {
		printf("%s: Unknown error %#x. Doing warm reboot.\n", __func__,
		       res);
		reboot();
	}

	vboot_boot_kernel(&kparams);

	return 1;
}

void vboot_boot_kernel(struct vb2_kernel_params *kparams)
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

	post_code(POST_CODE_CROSSYSTEM_SETUP);

	if (crossystem_setup(FIRMWARE_TYPE_AUTO_DETECT))
		return;

	boot(&bi);

 fail:
	/*
	 * If the boot succeeded we'd never end up here. If configured, let's
	 * try booting in alternative way.
	 */
	if (CONFIG(KERNEL_LEGACY))
		legacy_boot(bi.kernel, cmd_line_buf);
	if (CONFIG(KERNEL_MULTIBOOT))
		multiboot_boot(&bi);
}

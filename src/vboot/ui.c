// SPDX-License-Identifier: GPL-2.0

#include <vb2_api.h>
#include <vboot_api.h>

#include "drivers/storage/blockdev.h"
#include "vboot/load_kernel.h"
#include "vboot/stages.h"
#include "vboot/ui.h"

static vb2_error_t ui_broken_screen_action(struct ui_context *ui)
{
	/* Broken screen keyboard shortcuts */
	if (ui->key == '\t')
		return ui_screen_change(ui, UI_SCREEN_DEBUG_INFO);

	return VB2_SUCCESS;
}

static vb2_error_t ui_manual_recovery_action(struct ui_context *ui)
{
	/* See if we have a recovery kernel available yet. */
	vb2_error_t rv = vboot_load_kernel(ui->ctx, BLOCKDEV_REMOVABLE,
					   ui->kparams);
	if (rv == VB2_SUCCESS)
		return VB2_REQUEST_UI_EXIT;

	/* If disk validity state changed, switch to appropriate screen. */
	if (ui->recovery_rv != rv) {
		UI_INFO("Recovery vboot_load_kernel %#x --> %#x\n",
			ui->recovery_rv, rv);
		ui->recovery_rv = rv;
		return ui_screen_change(ui,
					rv == VB2_ERROR_LK_NO_DISK_FOUND
						? UI_SCREEN_RECOVERY_SELECT
						: UI_SCREEN_RECOVERY_INVALID);
	}

	/* Manual recovery keyboard shortcuts */
	if (ui->key == UI_KEY_REC_TO_DEV ||
	    (CONFIG(DETACHABLE) &&
	     ui->key == UI_BUTTON_VOL_UP_DOWN_COMBO_PRESS)) {
		return ui_screen_change(ui, UI_SCREEN_RECOVERY_TO_DEV);
	}

	if (ui->key == UI_KEY_INTERNET_RECOVERY)
		return ui_recovery_mode_boot_minios_action(ui);

	if (ui->key == '\t')
		return ui_screen_change(ui, UI_SCREEN_DEBUG_INFO);

	return VB2_SUCCESS;
}

static vb2_error_t developer_action(struct ui_context *ui)
{
	/*
	 * MMC controllers rely on update() calls to handle SD card insertion or
	 * removal. The current implementation of update(), however, cannot tell
	 * if the inserted SD card has been replaced with another one.
	 * Therefore, keep track of the MMC state in this global action.
	 *
	 * We don't need this for recovery UI, where the vboot_load_kernel()
	 * call already updates the removable block devices.
	 */
	if (ui->ctx->flags & VB2_CONTEXT_DEV_BOOT_ALLOWED)
		get_all_bdevs(BLOCKDEV_REMOVABLE, NULL);

	/* Developer mode keyboard shortcuts */
	if (ui->key == '\t')
		return ui_screen_change(ui, UI_SCREEN_DEBUG_INFO);

	/* Ignore other shortcuts */
	if (!(ui->ctx->flags & VB2_CONTEXT_DEV_BOOT_ALLOWED))
		return VB2_REQUEST_UI_CONTINUE;

	if (ui->key == UI_KEY_DEV_TO_NORM)
		return ui_screen_change(ui, UI_SCREEN_DEVELOPER_TO_NORM);
	if (ui->key == UI_KEY_DEV_BOOT_EXTERNAL ||
	    (CONFIG(DETACHABLE) && ui->key == UI_BUTTON_VOL_UP_LONG_PRESS))
		return ui_developer_mode_boot_external_action(ui);
	if (ui->key == UI_KEY_DEV_BOOT_INTERNAL ||
	    (CONFIG(DETACHABLE) && ui->key == UI_BUTTON_VOL_DOWN_LONG_PRESS))
		return ui_developer_mode_boot_internal_action(ui);
	if (ui->key == UI_KEY_DEV_BOOT_ALTFW)
		return ui_developer_mode_boot_altfw_action(ui);
	if (ui->key == UI_KEY_DEV_FASTBOOT)
		return ui_developer_mode_enter_fastboot_action(ui);

	return VB2_SUCCESS;
}

vb2_error_t vboot_select_and_load_kernel(struct vb2_context *ctx,
					 struct vb2_kernel_params *kparams)
{
	enum ui_screen root_screen_id = UI_SCREEN_RECOVERY_BROKEN;
	switch (ctx->boot_mode) {
	case VB2_BOOT_MODE_MANUAL_RECOVERY:
		VB2_TRY(ui_loop(ctx, UI_SCREEN_RECOVERY_SELECT,
				ui_manual_recovery_action, kparams));
		break;
	case VB2_BOOT_MODE_BROKEN_SCREEN:
		/*
		 * In EFS2, recovery mode can be entered even when battery is
		 * drained or damaged. EC-RO sets NO_BOOT flag in such case and
		 * uses PD power to boot AP.
		 *
		 * TODO: Inform user why recovery failed to start.
		 */
		if (ctx->flags & VB2_CONTEXT_NO_BOOT)
			UI_WARN("NO_BOOT in broken screen mode\n");

		VB2_TRY(ui_loop(ctx, UI_SCREEN_RECOVERY_BROKEN,
				ui_broken_screen_action, kparams));
		break;
	case VB2_BOOT_MODE_DIAGNOSTICS:
		VB2_TRY(ui_loop(ctx, UI_SCREEN_DIAGNOSTICS, NULL, kparams));
		/*
		 * The diagnostic menu should either boot a rom, or
		 * return either of reboot or shutdown.
		 */
		return VB2_REQUEST_REBOOT;
	case VB2_BOOT_MODE_DEVELOPER:
		root_screen_id = UI_SCREEN_DEVELOPER_MODE;
		if (!(ctx->flags & VB2_CONTEXT_DEV_BOOT_ALLOWED)) {
			UI_WARN("WARNING: "
				"Dev boot not allowed; forcing to-norm\n");
			root_screen_id = UI_SCREEN_DEVELOPER_TO_NORM;
		}
		VB2_TRY(ui_loop(ctx, root_screen_id, developer_action,
				kparams));
		break;
	case VB2_BOOT_MODE_NORMAL:
		VB2_TRY(vboot_load_kernel(ctx, BLOCKDEV_FIXED, kparams));
		break;
	default:
		return VB2_ERROR_ESCAPE_NO_BOOT;
	}
	return VB2_SUCCESS;
}

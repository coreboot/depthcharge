// SPDX-License-Identifier: GPL-2.0

#include "vboot/ui.h"
#include <vboot_api.h>
#include <vb2_api.h>

static vb2_error_t ui_broken_screen_action(struct ui_context *ui)
{
	/* Broken screen keyboard shortcuts */
	if (ui->key == '\t')
		return ui_screen_change(ui, VB2_SCREEN_DEBUG_INFO);

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_broken_screen_ui(struct vb2_context *ctx)
{
	return ui_loop(ctx, VB2_SCREEN_RECOVERY_BROKEN,
		       ui_broken_screen_action);
}

static vb2_error_t ui_manual_recovery_action(struct ui_context *ui)
{
	/* See if we have a recovery kernel available yet. */
	vb2_error_t rv = VbTryLoadKernel(ui->ctx, VB_DISK_FLAG_REMOVABLE);
	if (rv == VB2_SUCCESS)
		return VB2_REQUEST_UI_EXIT;

	/* If disk validity state changed, switch to appropriate screen. */
	if (ui->recovery_rv != rv) {
		UI_INFO("Recovery VbTryLoadKernel %#x --> %#x\n",
			ui->recovery_rv, rv);
		ui->recovery_rv = rv;
		return ui_screen_change(ui,
					rv == VB2_ERROR_LK_NO_DISK_FOUND
						? VB2_SCREEN_RECOVERY_SELECT
						: VB2_SCREEN_RECOVERY_INVALID);
	}

	/* Manual recovery keyboard shortcuts */
	if (ui->key == UI_KEY_REC_TO_DEV ||
	    (CONFIG(DETACHABLE) &&
	     ui->key == VB_BUTTON_VOL_UP_DOWN_COMBO_PRESS)) {
		return ui_screen_change(ui, VB2_SCREEN_RECOVERY_TO_DEV);
	}

	if (ui->key == '\t')
		return ui_screen_change(ui, VB2_SCREEN_DEBUG_INFO);

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_manual_recovery_ui(struct vb2_context *ctx)
{
	return ui_loop(ctx, VB2_SCREEN_RECOVERY_SELECT,
		       ui_manual_recovery_action);
}

static vb2_error_t developer_action(struct ui_context *ui)
{
	/* Developer mode keyboard shortcuts */
	if (ui->key == '\t')
		return ui_screen_change(ui, VB2_SCREEN_DEBUG_INFO);

	/* Ignore other shortcuts */
	if (!(ui->ctx->flags & VB2_CONTEXT_DEV_BOOT_ALLOWED))
		return VB2_REQUEST_UI_CONTINUE;

	if (ui->key == UI_KEY_DEV_TO_NORM)
		return ui_screen_change(ui, VB2_SCREEN_DEVELOPER_TO_NORM);
	if (ui->key == UI_KEY_DEV_BOOT_EXTERNAL ||
	    (CONFIG(DETACHABLE) && ui->key == VB_BUTTON_VOL_UP_LONG_PRESS))
		return ui_developer_mode_boot_external_action(ui);
	if (ui->key == UI_KEY_DEV_BOOT_INTERNAL ||
	    (CONFIG(DETACHABLE) && ui->key == VB_BUTTON_VOL_DOWN_LONG_PRESS))
		return ui_developer_mode_boot_internal_action(ui);
	if (ui->key == UI_KEY_DEV_BOOT_ALTFW)
		return ui_developer_mode_boot_altfw_action(ui);

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_developer_ui(struct vb2_context *ctx)
{
	enum vb2_screen root_screen_id = VB2_SCREEN_DEVELOPER_MODE;
	if (!(ctx->flags & VB2_CONTEXT_DEV_BOOT_ALLOWED)) {
		UI_WARN("WARNING: Dev boot not allowed; forcing to-norm\n");
		root_screen_id = VB2_SCREEN_DEVELOPER_TO_NORM;
	}
	return ui_loop(ctx, root_screen_id, developer_action);
}

vb2_error_t vb2ex_diagnostic_ui(struct vb2_context *ctx)
{
	return ui_loop(ctx, VB2_SCREEN_DIAGNOSTICS, NULL);
}

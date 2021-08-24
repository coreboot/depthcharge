// SPDX-License-Identifier: GPL-2.0

#include "vboot/ui.h"

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

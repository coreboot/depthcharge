// SPDX-License-Identifier: GPL-2.0

#include <vb2_api.h>

#include "drivers/video/display.h"
#include "vboot/ui.h"

vb2_error_t ui_display(struct ui_context *ui,
		       const struct ui_state *prev_state)
{
	if (CONFIG(HEADLESS))
		display_screen(ui->state->screen->id);
	return VB2_SUCCESS;
}

int ui_display_clear(void)
{
	return 0;
}

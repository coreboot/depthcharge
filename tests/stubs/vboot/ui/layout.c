// SPDX-License-Identifier: GPL-2.0

#include <vboot/ui.h>
#include <vb2_api.h>

vb2_error_t ui_draw_language_header(const struct ui_locale *locale,
				    const struct ui_state *state, int focused)
{
	return VB2_SUCCESS;
}

vb2_error_t ui_draw_menu_items(const struct ui_menu *menu,
			       const struct ui_state *state,
			       const struct ui_state *prev_state,
			       int32_t y)
{
	return VB2_SUCCESS;
}

vb2_error_t ui_draw_desc(const struct ui_desc *desc,
			 const struct ui_state *state,
			 int32_t *y)
{
	return VB2_SUCCESS;
}

vb2_error_t ui_draw_default(const struct ui_state *state,
			    const struct ui_state *prev_state)
{
	return VB2_SUCCESS;
}

vb2_error_t ui_draw_log_textbox(const char *str, const struct ui_state *state,
				int32_t *y)
{
	return VB2_SUCCESS;
}

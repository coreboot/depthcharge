// SPDX-License-Identifier: GPL-2.0

#include <vb2_api.h>

#include "vboot/ui.h"

const struct ui_menu *ui_get_menu(struct ui_context *ui)
{
	return VB2_SUCCESS;
}

vb2_error_t ui_menu_select(struct ui_context *ui)
{
	return VB2_SUCCESS;
}

vb2_error_t ui_screen_change(struct ui_context *ui, enum ui_screen id)
{
	return VB2_SUCCESS;
}

vb2_error_t ui_screen_back(struct ui_context *ui)
{
	return VB2_SUCCESS;
}

// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <vboot/ui.h>
#include <vb2_api.h>

vb2_error_t ui_display(enum vb2_screen screen, uint32_t locale_id,
		       uint32_t selected_item, uint32_t disabled_item_mask,
		       uint32_t hidden_item_mask, int timer_disabled,
		       uint32_t current_page, enum vb2_ui_error error_code)
{
	check_expected(screen);
	check_expected(locale_id);
	check_expected(selected_item);
	check_expected(disabled_item_mask);
	check_expected(hidden_item_mask);
	check_expected(current_page);
	return VB2_SUCCESS;
}

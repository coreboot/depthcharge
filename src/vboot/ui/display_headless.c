// SPDX-License-Identifier: GPL-2.0

#include <vb2_api.h>

#include "vboot/ui.h"

vb2_error_t ui_display(enum vb2_screen screen, uint32_t locale_id,
		       uint32_t selected_item, uint32_t disabled_item_mask,
		       uint32_t hidden_item_mask, int timer_disabled,
		       uint32_t current_page, enum vb2_ui_error error_code)
{
	return VB2_SUCCESS;
}

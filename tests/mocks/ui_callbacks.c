// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <mocks/callbacks.h>
#include <vboot_api.h>
#include <vb2_api.h>

uint32_t mock_time_ms;

uint32_t VbExIsShutdownRequested(void)
{
	return mock_type(uint32_t);
}

uint32_t VbExKeyboardRead(void)
{
	return mock_type(uint32_t);
}

uint32_t VbExKeyboardReadWithFlags(uint32_t *key_flags)
{
	*key_flags = mock_type(uint32_t);
	return mock_type(uint32_t);
}

uint32_t vb2ex_mtime(void)
{
	return mock_time_ms;
}

void vb2ex_msleep(uint32_t msec)
{
	mock_time_ms += msec;
}

vb2_error_t vb2ex_display_ui(enum vb2_screen screen, uint32_t locale_id,
			     uint32_t selected_item,
			     uint32_t disabled_item_mask,
			     uint32_t hidden_item_mask, int timer_disabled,
			     uint32_t current_page,
			     enum vb2_ui_error error_code)
{
	check_expected(screen);
	check_expected(locale_id);
	check_expected(selected_item);
	check_expected(disabled_item_mask);
	check_expected(hidden_item_mask);
	check_expected(current_page);
	return VB2_SUCCESS;
}

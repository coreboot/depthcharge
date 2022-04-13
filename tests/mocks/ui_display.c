// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <tests/vboot/ui/common.h>
#include <vboot/ui.h>
#include <vb2_api.h>

vb2_error_t _ui_display(enum ui_screen screen, uint32_t locale_id,
			uint32_t selected_item, uint32_t disabled_item_mask,
			uint32_t hidden_item_mask, int timer_disabled,
			uint32_t current_page, enum ui_error error_code)
{
	check_expected(screen);
	check_expected(locale_id);
	check_expected(selected_item);
	check_expected(disabled_item_mask);
	check_expected(hidden_item_mask);
	check_expected(current_page);
	return VB2_SUCCESS;
}

vb2_error_t ui_display(struct ui_context *ui,
		       const struct ui_state *prev_state)
{
	const struct ui_state *state;
	assert_non_null(ui);
	state = ui->state;
	assert_non_null(state);
	assert_non_null(state->screen);
	assert_non_null(state->locale);
	return _ui_display(state->screen->id, state->locale->id,
			   state->selected_item, state->disabled_item_mask,
			   state->hidden_item_mask, state->timer_disabled,
			   state->current_page, state->error_code);
}

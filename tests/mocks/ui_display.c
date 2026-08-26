// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <tests/vboot/ui/common.h>
#include <vboot/ui.h>
#include <vb2_api.h>

__weak void ui_display_side_effect(void)
{
	/* No-op function, do nothing. */
}

vb2_error_t _ui_display(enum ui_screen screen, uint32_t locale_id,
			uint32_t focused_item, uint32_t sub_focused_item,
			uint32_t disabled_item_mask, uint32_t hidden_item_mask,
			int timer_disabled, uint32_t current_page,
			enum ui_error error_code)
{
	check_expected(screen);
	check_expected(locale_id);
	check_expected(focused_item);
	check_expected(sub_focused_item);
	check_expected(disabled_item_mask);
	check_expected(hidden_item_mask);
	check_expected(current_page);
	check_expected(error_code);
	return VB2_SUCCESS;
}

vb2_error_t ui_display(struct ui_context *ui,
		       const struct ui_state *prev_state)
{
	const struct ui_state *state;
	const struct ui_menu_state *ms;
	const struct ui_menu_state *root_ms;
	uint32_t sub_focused_item;
	assert_non_null(ui);
	state = ui->state;
	assert_non_null(state);
	assert_non_null(state->screen);
	assert_non_null(state->locale);
	ms = ui_active_menu_state(ui->state);
	root_ms = &ui->state->menu_state;
	sub_focused_item = state->is_sub_menu_active ? ms->focused_item : UI_NO_SUBMENU;

	ui_display_side_effect();
	printf("%s: screen=%#x, locale=%u, focused_item=%u, "
	       "sub_focused_item=%u, disabled_item_mask=%#x, "
	       "hidden_item_mask=%#x, timer_disabled=%d, current_page=%u, "
	       "error=%#x\n",
	       __func__, state->screen->id, ui->state->locale->id,
	       root_ms->focused_item, sub_focused_item,
	       ms->disabled_item_mask, ms->hidden_item_mask,
	       state->timer_disabled, state->current_page,
	       state->error_code);
	return _ui_display(state->screen->id, state->locale->id,
			   root_ms->focused_item, sub_focused_item,
			   ms->disabled_item_mask, ms->hidden_item_mask,
			   state->timer_disabled, state->current_page,
			   state->error_code);
}

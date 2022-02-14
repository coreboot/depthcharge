// SPDX-License-Identifier: GPL-2.0

#include "vboot/ui.h"

static vb2_error_t default_screen_init(struct ui_context *ui)
{
	const struct ui_menu *menu = ui_get_menu(ui);
	ui->state->selected_item = 0;
	if (menu->num_items > 1 &&
	    menu->items[0].type == UI_MENU_ITEM_TYPE_LANGUAGE)
		ui->state->selected_item = 1;
	return VB2_SUCCESS;
}

/* This function assumes old_state is not NULL. */
static void preserve_state(struct ui_state *new_state,
			   const struct ui_state *old_state)
{
	new_state->locale = old_state->locale;
	new_state->timer_disabled = old_state->timer_disabled;
	new_state->error_code = old_state->error_code;
}

static void push_state(struct ui_context *ui, struct ui_state *state)
{
	if (ui->state)
		preserve_state(state, ui->state);
	state->prev = ui->state;
	ui->state = state;
}

/* This function assumes ui->state->prev is not NULL. */
static void pop_state(struct ui_context *ui)
{
	struct ui_state *tmp = ui->state;
	ui->state = ui->state->prev;
	preserve_state(ui->state, tmp);
	free(tmp);
}

vb2_error_t ui_screen_change(struct ui_context *ui, enum ui_screen id)
{
	const struct ui_screen_info *new_screen_info;
	struct ui_state *cur_state;
	int state_exists = 0;

	new_screen_info = ui_get_screen_info(id);
	if (new_screen_info == NULL) {
		UI_WARN("ERROR: Screen entry %#x not found; ignoring\n", id);
		return VB2_REQUEST_UI_CONTINUE;
	}

	/* Check to see if the screen state already exists in our stack. */
	cur_state = ui->state;
	while (cur_state != NULL) {
		if (cur_state->screen->id == id) {
			state_exists = 1;
			break;
		}
		cur_state = cur_state->prev;
	}

	if (state_exists) {
		/* Pop until the requested screen is at the top of stack. */
		while (ui->state->screen->id != id)
			pop_state(ui);
		if (ui->state->screen->reinit)
			VB2_TRY(ui->state->screen->reinit(ui));
	} else {
		/* Allocate the requested screen on top of the stack. */
		cur_state = malloc(sizeof(*ui->state));
		if (cur_state == NULL) {
			UI_WARN("WARNING: malloc failed; ignoring\n");
			return VB2_REQUEST_UI_CONTINUE;
		}
		memset(cur_state, 0, sizeof(*ui->state));
		cur_state->screen = new_screen_info;
		push_state(ui, cur_state);
		if (ui->state->screen->init)
			VB2_TRY(ui->state->screen->init(ui));
		else
			VB2_TRY(default_screen_init(ui));
	}

	return VB2_REQUEST_UI_CONTINUE;
}

vb2_error_t ui_screen_back(struct ui_context *ui)
{
	if (ui->state && ui->state->prev) {
		pop_state(ui);
		if (ui->state->screen->reinit)
			VB2_TRY(ui->state->screen->reinit(ui));
	} else {
		UI_WARN("ERROR: No previous screen; ignoring\n");
	}
	return VB2_REQUEST_UI_CONTINUE;
}

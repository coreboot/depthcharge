// SPDX-License-Identifier: GPL-2.0

#include <vboot_api.h>

#include "vboot/ui.h"

const struct ui_menu *ui_get_menu(struct ui_context *ui)
{
	const struct ui_menu *menu;
	static const struct ui_menu empty_menu = {
		.num_items = 0,
		.items = NULL,
	};
	if (ui->state->screen->get_menu) {
		menu = ui->state->screen->get_menu(ui);
		return menu ? menu : &empty_menu;
	} else {
		return &ui->state->screen->menu;
	}
}

vb2_error_t ui_menu_state_init(struct ui_context *ui,
			       struct ui_menu_state *ms,
			       const struct ui_menu *menu)
{
	memset(ms, 0, sizeof(*ms));
	ms->menu = menu;

	if (menu && menu->init)
		return menu->init(ui);

	return VB2_SUCCESS;
}

static vb2_error_t ui_menu_open_sub_menu(struct ui_context *ui,
					 const struct ui_menu *sub_menu)
{
	if (!sub_menu || sub_menu->num_items == 0)
		return VB2_SUCCESS;

	if (ui->state->is_sub_menu_active) {
		UI_WARN("Sub-menu already active; cannot open sub-menu\n");
		return VB2_SUCCESS;
	}

	ui->state->is_sub_menu_active = true;
	return ui_menu_state_init(ui, &ui->state->sub_menu_state, sub_menu);
}

vb2_error_t ui_menu_close_sub_menu(struct ui_context *ui)
{
	if (!ui->state->is_sub_menu_active) {
		UI_WARN("Sub-menu not active; cannot close sub-menu\n");
		return VB2_SUCCESS;
	}

	ui->state->is_sub_menu_active = false;
	return VB2_SUCCESS;
}

vb2_error_t ui_menu_prev(struct ui_context *ui)
{
	struct ui_menu_state *ms = ui_active_menu_state(ui->state);
	int item;

	item = (int)ms->focused_item - 1;
	while (item >= 0 && UI_GET_BIT(ms->hidden_item_mask, item))
		item--;
	/* Only update if item is valid */
	if (item >= 0)
		ms->focused_item = item;

	return VB2_SUCCESS;
}

vb2_error_t ui_menu_next(struct ui_context *ui)
{
	struct ui_menu_state *ms = ui_active_menu_state(ui->state);
	const struct ui_menu *menu = ms->menu;
	int item;

	item = (int)ms->focused_item + 1;
	while (item < menu->num_items &&
	       UI_GET_BIT(ms->hidden_item_mask, item))
		item++;
	/* Only update if item is valid */
	if (item < menu->num_items)
		ms->focused_item = item;

	return VB2_SUCCESS;
}

vb2_error_t ui_menu_select(struct ui_context *ui)
{
	struct ui_menu_state *ms = ui_active_menu_state(ui->state);
	const struct ui_menu *menu = ms->menu;
	const struct ui_menu_item *menu_item;

	if (menu->num_items == 0)
		return VB2_SUCCESS;

	menu_item = &menu->items[ms->focused_item];

	/* Cannot select a disabled menu item */
	if (UI_GET_BIT(ms->disabled_item_mask,
		       ms->focused_item)) {
		UI_WARN("Menu item <%s> disabled; ignoring\n",
			menu_item->name);
		return VB2_SUCCESS;
	}

	if (menu_item->sub_menu) {
		UI_INFO("Menu item <%s> open sub-menu\n", menu_item->name);
		return ui_menu_open_sub_menu(ui, menu_item->sub_menu);
	}

	if (menu_item->action) {
		UI_INFO("Menu item <%s> run action\n", menu_item->name);
		return menu_item->action(ui);
	} else if (menu_item->target) {
		UI_INFO("Menu item <%s> to target screen %#x\n",
			menu_item->name, menu_item->target);
		return ui_screen_change(ui, menu_item->target);
	}

	UI_WARN("Menu item <%s> no action or target screen\n",
		menu_item->name);
	return VB2_SUCCESS;
}

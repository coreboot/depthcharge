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

vb2_error_t ui_menu_prev(struct ui_context *ui)
{
	int item;

	item = ui->state->focused_item - 1;
	while (item >= 0 && UI_GET_BIT(ui->state->hidden_item_mask, item))
		item--;
	/* Only update if item is valid */
	if (item >= 0)
		ui->state->focused_item = item;

	return VB2_SUCCESS;
}

vb2_error_t ui_menu_next(struct ui_context *ui)
{
	int item;
	const struct ui_menu *menu;

	menu = ui_get_menu(ui);
	item = ui->state->focused_item + 1;
	while (item < menu->num_items &&
	       UI_GET_BIT(ui->state->hidden_item_mask, item))
		item++;
	/* Only update if item is valid */
	if (item < menu->num_items)
		ui->state->focused_item = item;

	return VB2_SUCCESS;
}

vb2_error_t ui_menu_select(struct ui_context *ui)
{
	const struct ui_menu *menu;
	const struct ui_menu_item *menu_item;

	menu = ui_get_menu(ui);
	if (menu->num_items == 0)
		return VB2_SUCCESS;

	menu_item = &menu->items[ui->state->focused_item];

	/* Cannot select a disabled menu item */
	if (UI_GET_BIT(ui->state->disabled_item_mask,
		       ui->state->focused_item)) {
		UI_WARN("Menu item <%s> disabled; ignoring\n",
			menu_item->name);
		return VB2_SUCCESS;
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

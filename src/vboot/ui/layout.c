// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>

#include "vboot/ui.h"
#include "vboot/util/commonparams.h"

vb2_error_t ui_draw_language_header(const struct ui_locale *locale,
				    const struct ui_state *state, int focused)
{
	int32_t x, y, y_center, w;
	const int reverse = state->locale->rtl;
	const int32_t box_width = UI_LANG_ICON_GLOBE_SIZE +
		UI_LANG_ICON_MARGIN_H * 2 + UI_LANG_TEXT_WIDTH +
		UI_LANG_ICON_ARROW_SIZE + UI_LANG_ICON_MARGIN_H;
	const int32_t box_height = UI_LANG_BOX_HEIGHT;
	const uint32_t flags = PIVOT_H_LEFT | PIVOT_V_CENTER;
	struct ui_bitmap bitmap;

	x = UI_MARGIN_H;
	y = UI_MARGIN_TOP;
	y_center = y + box_height / 2;

	VB2_TRY(ui_draw_rounded_box(x, y, box_width, box_height,
				    &ui_color_lang_header_bg,
				    0, UI_LANG_BORDER_RADIUS, reverse));

	/* Draw globe */
	x += UI_LANG_ICON_MARGIN_H;
	w = UI_LANG_ICON_GLOBE_SIZE;
	VB2_TRY(ui_get_bitmap("ic_globe.bmp", NULL, 0, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y_center, w, w, flags, reverse));
	x += w + UI_LANG_ICON_MARGIN_H;

	/* Draw language text */
	VB2_TRY(ui_get_language_name_bitmap(locale->code, 1, 0, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y_center,
			       UI_SIZE_AUTO, UI_LANG_TEXT_HEIGHT,
			       flags, reverse));
	x += UI_LANG_TEXT_WIDTH;

	/* Draw dropdown arrow */
	w = UI_LANG_ICON_ARROW_SIZE;
	VB2_TRY(ui_get_bitmap("ic_dropdown.bmp", NULL, 0, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y_center, w, w, flags, reverse));

	if (!focused)
		return VB2_SUCCESS;

	/* Draw box border */
	x = UI_MARGIN_H;
	y = UI_MARGIN_TOP;
	VB2_TRY(ui_draw_rounded_box(x, y, box_width, box_height,
				    &ui_color_lang_header_border,
				    UI_LANG_BORDER_THICKNESS,
				    UI_LANG_BORDER_RADIUS, reverse));

	return VB2_SUCCESS;
}

/*
 * Draw step icons.
 *
 * @param state		Current UI state.
 * @param prev_state	Previous UI state.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
static vb2_error_t ui_draw_step_icons(const struct ui_state *state,
				      const struct ui_state *prev_state)
{
	struct ui_bitmap bitmap;
	const struct ui_screen_info *screen = state->screen;
	const int has_error = screen->step < 0;
	const int cur_step = screen->step >= 0 ? screen->step : -screen->step;
	const int num_steps = screen->num_steps;
	int step;
	int32_t x = UI_MARGIN_H;
	const int32_t y = UI_MARGIN_TOP + UI_LANG_BOX_HEIGHT +
		UI_LANG_MARGIN_BOTTOM;
	const int32_t y_center = y + UI_ICON_HEIGHT / 2;
	const int32_t icon_size = UI_STEP_ICON_HEIGHT;
	const int reverse = state->locale->rtl;
	uint32_t flags = PIVOT_H_LEFT | PIVOT_V_CENTER;

	for (step = 1; step <= num_steps; step++) {
		if (has_error && step == cur_step)
			VB2_TRY(ui_get_bitmap("ic_error.bmp", NULL, 0,
					      &bitmap));
		else if (step < cur_step)
			VB2_TRY(ui_get_bitmap("ic_done.bmp", NULL, 0, &bitmap));
		else
			VB2_TRY(ui_get_step_icon_bitmap(step, step == cur_step,
							&bitmap));
		VB2_TRY(ui_draw_bitmap(&bitmap, x, y_center,
				       icon_size, icon_size, flags, reverse));
		x += icon_size;
		if (step == num_steps)
			break;

		/* Separator */
		x += UI_STEP_ICON_MARGIN_H;
		VB2_TRY(ui_draw_box(x, y_center,
				    UI_STEP_ICON_SEPARATOR_WIDTH, UI_SIZE_MIN,
				    &ui_color_border, reverse));
		x += UI_STEP_ICON_SEPARATOR_WIDTH + UI_STEP_ICON_MARGIN_H;
	}

	return VB2_SUCCESS;
}

static vb2_error_t draw_footer(const struct ui_state *state)
{
	char hwid[VB2_GBB_HWID_MAX_SIZE];
	uint32_t size = sizeof(hwid);
	if (vb2api_gbb_read_hwid(vboot_get_context(), hwid, &size) !=
	    VB2_SUCCESS)
		strcpy(hwid, "NOT FOUND");

	const char *locale_code = state->locale->code;
	const int reverse = state->locale->rtl;
	const uint32_t flags = PIVOT_H_LEFT | PIVOT_V_TOP;
	const int32_t text_height = UI_FOOTER_TEXT_HEIGHT;
	int32_t x, y, vspacing;
	int32_t col2_width;
	const int32_t w = UI_SIZE_AUTO;
	const int32_t footer_y = UI_SCALE - UI_MARGIN_BOTTOM - UI_FOOTER_HEIGHT;
	const int32_t footer_height = UI_FOOTER_HEIGHT;
	struct ui_bitmap bitmap;

	/* Column 1 */
	x = UI_MARGIN_H;
	y = footer_y;
	VB2_TRY(ui_get_bitmap("qr_rec.bmp", NULL, 0, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, footer_height, footer_height,
			       flags, reverse));
	x += footer_height + UI_FOOTER_COL1_MARGIN_RIGHT;

	/* Column 2: 4 lines of text */
	y = footer_y;
	vspacing = footer_height - text_height * 4 -
		UI_FOOTER_COL2_LINE_SPACING * 2;
	VB2_TRY(ui_get_bitmap("model.bmp", locale_code, 0, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, text_height, flags, reverse));
	y += text_height + UI_FOOTER_COL2_LINE_SPACING;
	VB2_TRY(ui_draw_text(hwid, x, y, text_height, flags, UI_CHAR_STYLE_DARK,
			     reverse));
	y += text_height + vspacing;
	VB2_TRY(ui_get_bitmap("help_center.bmp", locale_code, 0, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, text_height, flags, reverse));
	y += text_height + UI_FOOTER_COL2_LINE_SPACING;
	VB2_TRY(ui_get_bitmap("rec_url.bmp", locale_code, 0, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, text_height, flags, reverse));
	VB2_TRY(ui_get_bitmap_width(&bitmap, text_height, &col2_width));
	x += col2_width + UI_FOOTER_COL2_MARGIN_RIGHT;

	/* Separator */
	VB2_TRY(ui_draw_box(x, footer_y, UI_SIZE_MIN, footer_height,
			    &ui_color_border, reverse));

	/* Column 3: 2 lines of text */
	int32_t icon_width;
	const int32_t icon_height = UI_FOOTER_COL3_ICON_HEIGHT;
	x += UI_FOOTER_COL3_MARGIN_LEFT;
	y = footer_y;
	vspacing = footer_height - text_height * 2 - icon_height -
		UI_FOOTER_COL3_PARA_SPACING;
	VB2_TRY(ui_get_bitmap("to_navigate.bmp", locale_code, 0, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, text_height, flags, reverse));
	y += text_height + vspacing;

	VB2_TRY(ui_get_bitmap("navigate.bmp", locale_code, 0, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, text_height, flags, reverse));
	y += text_height + UI_FOOTER_COL3_PARA_SPACING;

	const char *const icon_files[] = {
		"nav-key_enter.bmp",
		"nav-key_up.bmp",
		"nav-key_down.bmp",
	};

	for (int i = 0; i < ARRAY_SIZE(icon_files); i++) {
		VB2_TRY(ui_get_bitmap(icon_files[i], NULL, 0, &bitmap));
		VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, icon_height,
				       flags, reverse));
		VB2_TRY(ui_get_bitmap_width(&bitmap, icon_height, &icon_width));
		x += icon_width + UI_FOOTER_COL3_ICON_SPACING;
	}

	return VB2_SUCCESS;
}

/*
 * Get button width, based on the longest text of all the buttons.
 *
 * @param menu			Menu items.
 * @param state			UI state.
 * @param button_width		Button width to be calculated.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
static vb2_error_t ui_get_button_width(const struct ui_menu *menu,
				       const struct ui_state *state,
				       int32_t *button_width)
{
	int i;
	struct ui_bitmap bitmap;
	int32_t text_width;
	int32_t max_text_width = 0;

	for (i = 0; i < menu->num_items; i++) {
		if (state->disabled_item_mask & (1 << i))
			continue;
		if (menu->items[i].type != UI_MENU_ITEM_TYPE_PRIMARY)
			continue;
		VB2_TRY(ui_get_bitmap(menu->items[i].file, state->locale->code,
				      0, &bitmap));
		VB2_TRY(ui_get_bitmap_width(&bitmap, UI_BUTTON_TEXT_HEIGHT,
					    &text_width));
		max_text_width = MAX(text_width, max_text_width);
	}

	*button_width = max_text_width + UI_BUTTON_TEXT_PADDING_H * 2;
	return VB2_SUCCESS;
}

/*
 * Draw a button.
 *
 * @param image_name	Image name.
 * @param locale_code	Language code of current locale.
 * @param x		x-coordinate of the top-left corner.
 * @param y		y-coordinate of the top-left corner.
 * @param width		Width of the box.
 * @param height	Height of the box.
 * @param reverse	Whether to reverse the x-coordinate relative to the
 *			canvas.
 * @param focused	1 for focused and 0 for non-focused.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
static vb2_error_t ui_draw_button(const char *image_name,
				  const char *locale_code,
				  int32_t x, int32_t y,
				  int32_t width, int32_t height,
				  int reverse, int focused)
{
	struct ui_bitmap bitmap;
	const int32_t x_center = x + width / 2;
	const int32_t y_center = y + height / 2;
	const uint32_t flags = PIVOT_H_CENTER | PIVOT_V_CENTER;

	/* Clear button area */
	VB2_TRY(ui_draw_rounded_box(x, y, width, height,
				    focused ? &ui_color_button : &ui_color_bg,
				    0, UI_BUTTON_BORDER_RADIUS,
				    reverse));

	/* Draw button text */
	VB2_TRY(ui_get_bitmap(image_name, locale_code, focused, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x_center, y_center,
			       UI_SIZE_AUTO, UI_BUTTON_TEXT_HEIGHT,
			       flags, reverse));

	/* Draw button borders */
	VB2_TRY(ui_draw_rounded_box(x, y, width, height,
				    &ui_color_button_border,
				    UI_BUTTON_BORDER_THICKNESS,
				    UI_BUTTON_BORDER_RADIUS, reverse));

	return VB2_SUCCESS;
}

/*
 * Draw a link button, where the style is different from a primary button.
 *
 * @param image_name	Image name.
 * @param locale_code	Language code of current locale.
 * @param x		x-coordinate of the top-left corner.
 * @param y		y-coordinate of the top-left corner.
 * @param height	Height of the box.
 * @param reverse	Whether to reverse the x-coordinate relative to the
 *			canvas.
 * @param focused	1 for focused and 0 for non-focused.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
static vb2_error_t ui_draw_link(const char *image_name,
				const char *locale_code,
				int32_t x, int32_t y, int32_t height,
				int reverse, int focused)
{
	struct ui_bitmap bitmap;
	int32_t text_width, width;
	const int32_t x_base = x;
	const int32_t y_center = y + height / 2;
	const uint32_t flags = PIVOT_H_LEFT | PIVOT_V_CENTER;
	const char *arrow_file;

	/* Get button width */
	VB2_TRY(ui_get_bitmap(image_name, locale_code, focused, &bitmap));
	VB2_TRY(ui_get_bitmap_width(&bitmap, UI_BUTTON_TEXT_HEIGHT,
				    &text_width));
	width = UI_LINK_TEXT_PADDING_LEFT + text_width +
		UI_LINK_ARROW_SIZE + UI_LINK_ARRAW_MARGIN_H * 2;

	/* Clear button area */
	VB2_TRY(ui_draw_rounded_box(x_base, y, width, height,
				    focused ? &ui_color_link_bg :
				    &ui_color_bg,
				    0, UI_BUTTON_BORDER_RADIUS,
				    reverse));

	/* Draw button text */
	x += UI_LINK_TEXT_PADDING_LEFT;
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y_center,
			       UI_SIZE_AUTO, UI_BUTTON_TEXT_HEIGHT,
			       flags, reverse));
	x += text_width;

	/* Draw arrow */
	x += UI_LINK_ARRAW_MARGIN_H;
	arrow_file = reverse ? "ic_dropleft.bmp" : "ic_dropright.bmp";
	VB2_TRY(ui_get_bitmap(arrow_file, NULL, focused, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y_center,
			       UI_LINK_ARROW_SIZE, UI_LINK_ARROW_SIZE,
			       flags, reverse));
	x += UI_LINK_ARROW_SIZE + UI_LINK_ARRAW_MARGIN_H;

	/* Draw button borders */
	if (focused)
		VB2_TRY(ui_draw_rounded_box(x_base, y, width, height,
					    &ui_color_button_border,
					    UI_LINK_BORDER_THICKNESS,
					    UI_BUTTON_BORDER_RADIUS, reverse));

	return VB2_SUCCESS;
}

vb2_error_t ui_draw_desc(const struct ui_desc *desc,
			 const struct ui_state *state,
			 int32_t *height)
{
	int i;
	struct ui_bitmap bitmap;
	const char *locale_code = state->locale->code;
	const int reverse = state->locale->rtl;
	int32_t x, y;
	const int32_t w = UI_SIZE_AUTO;
	const int32_t h = UI_DESC_TEXT_HEIGHT;
	uint32_t flags = PIVOT_H_LEFT | PIVOT_V_TOP;

	x = UI_MARGIN_H;
	y = UI_MARGIN_TOP + UI_LANG_BOX_HEIGHT + UI_LANG_MARGIN_BOTTOM +
		UI_ICON_HEIGHT + UI_ICON_MARGIN_BOTTOM +
		UI_TITLE_TEXT_HEIGHT + UI_TITLE_MARGIN_BOTTOM;
	*height = 0;

	for (i = 0; i < desc->count; i++) {
		VB2_TRY(ui_get_bitmap(desc->files[i], locale_code, 0, &bitmap));
		VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, h, flags, reverse));
		y += h + UI_DESC_TEXT_LINE_SPACING;
		*height += h + UI_DESC_TEXT_LINE_SPACING;
	}

	return VB2_SUCCESS;
}

vb2_error_t ui_draw_default(const struct ui_state *state,
			    const struct ui_state *prev_state)
{
	int i;
	const struct ui_screen_info *screen = state->screen;
	const struct ui_menu *menu = &screen->menu;
	const char *locale_code = state->locale->code;
	const int reverse = state->locale->rtl;
	int focused;
	int32_t x, y, desc_height;
	const int32_t w = UI_SIZE_AUTO;
	uint32_t flags = PIVOT_H_LEFT | PIVOT_V_TOP;
	const char *icon_file;
	struct ui_bitmap bitmap;

	if (!prev_state || prev_state->locale != state->locale) {
		/*
		 * Clear the whole screen if previous drawing failed, if there
		 * is no previous screen, or if locale changed.
		 */
		clear_screen(&ui_color_bg);
	} else if (prev_state->screen != state->screen) {
		/* Clear everything above the footer for new screen. */
		const int32_t box_height = UI_SCALE - UI_MARGIN_BOTTOM -
			UI_FOOTER_HEIGHT;
		/* TODO(b/147424699): Do not clear and redraw language header */
		VB2_TRY(ui_draw_box(0, 0, UI_SCALE, box_height,
				    &ui_color_bg, 0));
	}

	/* Language dropdown header */
	if (menu->num_items > 0 &&
	    menu->items[0].type == UI_MENU_ITEM_TYPE_LANGUAGE) {
		focused = state->selected_item == 0;
		if (!prev_state ||
		    prev_state->screen != state->screen ||
		    prev_state->locale != state->locale ||
		    (prev_state->selected_item == 0) != focused) {
			VB2_TRY(ui_draw_language_header(state->locale, state,
							focused));
		}
	}

	/*
	 * Draw the footer if previous screen doesn't have a footer, or if
	 * locale changed.
	 */
	if (!screen->no_footer &&
	    (!prev_state ||
	     prev_state->screen->no_footer ||
	     prev_state->locale != state->locale))
		VB2_TRY(draw_footer(state));

	/* Icon */
	switch (screen->icon) {
	case UI_ICON_TYPE_INFO:
		icon_file = "ic_info.bmp";
		break;
	case UI_ICON_TYPE_ERROR:
		icon_file = "ic_error.bmp";
		break;
	case UI_ICON_TYPE_DEV_MODE:
		icon_file = "ic_dev_mode.bmp";
		break;
	case UI_ICON_TYPE_RESTART:
		icon_file = "ic_restart.bmp";
		break;
	default:
		icon_file = NULL;
		break;
	}

	x = UI_MARGIN_H;
	y = UI_MARGIN_TOP + UI_LANG_BOX_HEIGHT + UI_LANG_MARGIN_BOTTOM;
	if (screen->icon == UI_ICON_TYPE_STEP) {
		VB2_TRY(ui_draw_step_icons(state, prev_state));
	} else if (icon_file) {
		VB2_TRY(ui_get_bitmap(icon_file, NULL, 0, &bitmap));
		VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, UI_ICON_HEIGHT,
				       flags, reverse));
	}
	y += UI_ICON_HEIGHT + UI_ICON_MARGIN_BOTTOM;

	/* Title */
	if (screen->title) {
		VB2_TRY(ui_get_bitmap(screen->title, locale_code, 0, &bitmap));
		VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, UI_TITLE_TEXT_HEIGHT,
				       flags, reverse));
	}
	y += UI_TITLE_TEXT_HEIGHT + UI_TITLE_MARGIN_BOTTOM;

	/* Description */
	if (screen->draw_desc)
		VB2_TRY(screen->draw_desc(state, prev_state, &desc_height));
	else
		VB2_TRY(ui_draw_desc(&screen->desc, state, &desc_height));
	y += desc_height + UI_DESC_MARGIN_BOTTOM;

	/* Buttons */
	int32_t button_width;
	VB2_TRY(ui_get_button_width(menu, state, &button_width));

	for (i = 0; i < menu->num_items; i++) {
		if (state->disabled_item_mask & (1 << i))
			continue;
		if (menu->items[i].type != UI_MENU_ITEM_TYPE_PRIMARY)
			continue;
		/*
		 * TODO(b/147424699): No need to redraw every button when
		 * navigating between menu.
		 */
		VB2_TRY(ui_draw_button(menu->items[i].file, locale_code,
				       x, y, button_width, UI_BUTTON_HEIGHT,
				       reverse, state->selected_item == i));
		y += UI_BUTTON_HEIGHT + UI_BUTTON_MARGIN_BOTTOM;
	}

	/* Advanced options */
	x = UI_MARGIN_H - UI_LINK_TEXT_PADDING_LEFT;
	y = UI_SCALE - UI_MARGIN_BOTTOM - UI_FOOTER_HEIGHT -
		UI_FOOTER_MARGIN_TOP - UI_BUTTON_HEIGHT;
	for (i = 0; i < menu->num_items; i++) {
		if (state->disabled_item_mask & (1 << i))
			continue;
		if (menu->items[i].type != UI_MENU_ITEM_TYPE_SECONDARY)
			continue;
		VB2_TRY(ui_draw_link(menu->items[i].file, locale_code,
				     x, y, UI_BUTTON_HEIGHT, reverse,
				     state->selected_item == i));
		y += UI_BUTTON_HEIGHT + UI_BUTTON_MARGIN_BOTTOM;
	}

	return VB2_SUCCESS;
}

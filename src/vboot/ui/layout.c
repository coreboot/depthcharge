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
	VB2_TRY(ui_get_language_name_bitmap(locale->code, &bitmap));
	VB2_TRY(ui_draw_mapped_bitmap(&bitmap, x, y_center,
				      UI_SIZE_AUTO, UI_LANG_TEXT_HEIGHT,
				      &ui_color_lang_header_bg, &ui_color_fg,
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
	char *p;
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

	/* hwid */
	if (vb2api_gbb_read_hwid(vboot_get_context(), hwid, &size) ==
	    VB2_SUCCESS) {
		/* Truncate everything after the first whitespace. */
		p = strchr(hwid, ' ');
		if (p)
			*p = '\0';
	} else {
		strcpy(hwid, "NOT FOUND");
	}

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
	VB2_TRY(ui_draw_text(hwid, x, y, text_height,
			     &ui_color_bg, &ui_color_footer_fg,
			     flags, reverse));
	y += text_height + vspacing;
	VB2_TRY(ui_get_bitmap("help_center.bmp", locale_code, 0, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, text_height, flags, reverse));
	y += text_height + UI_FOOTER_COL2_LINE_SPACING;
	VB2_TRY(ui_get_bitmap("rec_url.bmp", NULL, 0, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, text_height, flags, reverse));
	VB2_TRY(ui_get_bitmap_width(&bitmap, text_height, &col2_width));
	x += col2_width + UI_FOOTER_COL2_MARGIN_RIGHT;

	/* Separator */
	VB2_TRY(ui_draw_box(x, footer_y, UI_SIZE_MIN, footer_height,
			    &ui_color_border, reverse));

	/*
	 * Column 3 contains:
	 * - Navigation instruction string 1 (possibly multi-line)
	 * - Navigation instruction string 2 (possibly multi-line)
	 * - Navigation icons (one line)
	 * vspacing is the space between 2 navigation strings, while
	 * para_spacing is the space before the icon line.
	 */
	int32_t icon_width;
	int32_t para_spacing;
	uint32_t col3_num_lines0, col3_num_lines1;
	const int32_t icon_height = UI_FOOTER_COL3_ICON_HEIGHT;
	struct ui_bitmap col3_bitmap0, col3_bitmap1;
	VB2_TRY(ui_get_bitmap("navigate0.bmp", locale_code, 0, &col3_bitmap0));
	VB2_TRY(ui_get_bitmap("navigate1.bmp", locale_code, 0, &col3_bitmap1));
	col3_num_lines0 = ui_get_bitmap_num_lines(&col3_bitmap0);
	col3_num_lines1 = ui_get_bitmap_num_lines(&col3_bitmap1);
	x += UI_FOOTER_COL3_MARGIN_LEFT;
	y = footer_y;
	para_spacing = UI_FOOTER_COL3_PARA_SPACING;
	vspacing = footer_height -
		text_height * (col3_num_lines0 + col3_num_lines1) -
		icon_height - UI_FOOTER_COL3_PARA_SPACING;
	/* Not enough room for the vspacing; reduce the para_spacing */
	if (vspacing < 0) {
		para_spacing = MAX(para_spacing + vspacing -
				   UI_FOOTER_COL3_SPACING_MIN,
				   UI_FOOTER_COL3_SPACING_MIN);
		vspacing = UI_FOOTER_COL3_SPACING_MIN;
	}

	VB2_TRY(ui_draw_bitmap(&col3_bitmap0, x, y, w,
			       text_height * col3_num_lines0, flags, reverse));
	y += text_height * col3_num_lines0 + vspacing;

	VB2_TRY(ui_draw_bitmap(&col3_bitmap1, x, y, w,
			       text_height * col3_num_lines1, flags, reverse));
	y += text_height * col3_num_lines1 + para_spacing;

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

vb2_error_t ui_get_button_width(const struct ui_menu *menu,
				const struct ui_state *state,
				int32_t *button_width)
{
	int i;
	struct ui_bitmap bitmap;
	int32_t text_width;
	int32_t max_text_width = 0;

	for (i = 0; i < menu->num_items; i++) {
		if (menu->items[i].type != UI_MENU_ITEM_TYPE_PRIMARY)
			continue;
		if (!(menu->items[i].flags & UI_MENU_ITEM_FLAG_TRANSIENT) &&
		    VB2_GET_BIT(state->hidden_item_mask, i))
			continue;
		if (menu->items[i].file) {
			VB2_TRY(ui_get_bitmap(menu->items[i].file,
					      state->locale->code, 0, &bitmap));
			VB2_TRY(ui_get_bitmap_width(&bitmap,
						    UI_BUTTON_TEXT_HEIGHT,
						    &text_width));
		} else if (menu->items[i].text) {
			VB2_TRY(ui_get_text_width(menu->items[i].text,
						  UI_BUTTON_TEXT_HEIGHT,
						  &text_width));
		} else {
			UI_ERROR("Menu item #%d: no .file or .text\n", i);
			return VB2_ERROR_UI_DRAW_FAILURE;
		}
		max_text_width = MAX(text_width, max_text_width);
	}

	*button_width = max_text_width + UI_BUTTON_TEXT_PADDING_H * 2;
	return VB2_SUCCESS;
}

vb2_error_t ui_draw_button(const struct ui_menu_item *item,
			   const char *locale_code,
			   int32_t x, int32_t y,
			   int32_t width, int32_t height,
			   int reverse, int focused, int disabled,
			   int clear_help)
{
	struct ui_bitmap bitmap;
	const int32_t x_center = x + width / 2;
	const int32_t y_center = y + height / 2;
	const uint32_t flags = PIVOT_H_CENTER | PIVOT_V_CENTER;
	const struct rgb_color *bg_color, *fg_color, *border_color;
	int32_t border_thickness;

	/* Set button styles */
	if (focused) {
		if (disabled) {
			bg_color = &ui_color_button_disabled_bg;
			fg_color = &ui_color_button_disabled_fg;
		} else {
			bg_color = &ui_color_button;
			fg_color = &ui_color_bg;
		}
		/* Focus ring */
		border_color = &ui_color_button_focus_ring;
		border_thickness = UI_BUTTON_FOCUS_RING_THICKNESS;
	} else {
		if (disabled) {
			bg_color = &ui_color_bg;
			fg_color = &ui_color_button_disabled_fg;
		} else {
			bg_color = &ui_color_bg;
			fg_color = &ui_color_button;
		}
		/* Regular button border */
		border_color = &ui_color_button_border;
		border_thickness = UI_BUTTON_BORDER_THICKNESS;
	}

	/* Clear button area */
	VB2_TRY(ui_draw_rounded_box(x, y, width, height, bg_color,
				    0, UI_BUTTON_BORDER_RADIUS,
				    reverse));

	/* Draw button borders */
	VB2_TRY(ui_draw_rounded_box(x, y, width, height,
				    border_color, border_thickness,
				    UI_BUTTON_BORDER_RADIUS, reverse));

	/* Draw button text */
	if (item->file) {
		VB2_TRY(ui_get_bitmap(item->file, locale_code, 0, &bitmap));
		VB2_TRY(ui_draw_mapped_bitmap(&bitmap, x_center, y_center,
					      UI_SIZE_AUTO,
					      UI_BUTTON_TEXT_HEIGHT,
					      bg_color, fg_color,
					      flags, reverse));
	} else if (item->text) {
		VB2_TRY(ui_draw_text(item->text, x_center, y_center,
				     UI_BUTTON_TEXT_HEIGHT,
				     bg_color, fg_color,
				     flags, reverse));
	} else {
		UI_ERROR("No button image filename or text\n");
		return VB2_ERROR_UI_DRAW_FAILURE;
	}

	/* Draw disabled help text if the disabled button is on focus;
	   clear the area only if needed. */
	if (item->disabled_help_text_file) {
		const int32_t x_help = x + width + UI_BUTTON_HELP_TEXT_MARGIN_L;
		int32_t help_text_width;
		if (disabled && focused) {
			VB2_TRY(ui_get_bitmap(
					item->disabled_help_text_file,
					locale_code, 0, &bitmap));
			VB2_TRY(ui_draw_mapped_bitmap(
					&bitmap,
					x_help, y_center,
					UI_SIZE_AUTO, UI_BUTTON_TEXT_HEIGHT,
					&ui_color_bg, &ui_color_button_help_fg,
					PIVOT_H_LEFT | PIVOT_V_CENTER,
					reverse));
		} else if (clear_help) {
			VB2_TRY(ui_get_bitmap(
					item->disabled_help_text_file,
					locale_code, 0, &bitmap));
			VB2_TRY(ui_get_bitmap_width(
					&bitmap,
					UI_BUTTON_TEXT_HEIGHT,
					&help_text_width));
			VB2_TRY(ui_draw_box(
					x_help,
					y_center - UI_BUTTON_TEXT_HEIGHT / 2,
					help_text_width, UI_BUTTON_TEXT_HEIGHT,
					&ui_color_bg,
					reverse));
		}
	}

	return VB2_SUCCESS;
}

/*
 * Draw a link button, where the style is different from a primary button.
 *
 * @param item		Menu item.
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
static vb2_error_t ui_draw_link(const struct ui_menu_item *item,
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
	const struct rgb_color *bg_color;

	bg_color = focused ? &ui_color_link_bg : &ui_color_bg;

	/* Get button width */
	VB2_TRY(ui_get_bitmap(item->file, locale_code, 0, &bitmap));
	VB2_TRY(ui_get_bitmap_width(&bitmap, UI_BUTTON_TEXT_HEIGHT,
				    &text_width));
	width = UI_LINK_TEXT_PADDING_LEFT +
		UI_LINK_ICON_SIZE + UI_LINK_ICON_MARGIN_R +
		text_width + UI_LINK_ARROW_MARGIN_H;
	if (!(item->flags & UI_MENU_ITEM_FLAG_NO_ARROW))
		width += UI_LINK_ARROW_SIZE + UI_LINK_ARROW_MARGIN_H;

	/* Clear button area */
	VB2_TRY(ui_draw_rounded_box(x_base, y, width, height,
				    bg_color, 0, UI_BUTTON_BORDER_RADIUS,
				    reverse));

	/* Draw button icon */
	x += UI_LINK_TEXT_PADDING_LEFT;
	if (item->icon_file) {
		VB2_TRY(ui_get_bitmap(item->icon_file, NULL, focused, &bitmap));
		VB2_TRY(ui_draw_bitmap(&bitmap, x, y_center,
				       UI_LINK_ICON_SIZE, UI_LINK_ICON_SIZE,
				       flags, reverse));
	}
	x += UI_LINK_ICON_SIZE + UI_LINK_ICON_MARGIN_R;

	/* Draw button text */
	VB2_TRY(ui_get_bitmap(item->file, locale_code, 0, &bitmap));
	VB2_TRY(ui_draw_mapped_bitmap(&bitmap, x, y_center,
				      UI_SIZE_AUTO, UI_BUTTON_TEXT_HEIGHT,
				      bg_color, &ui_color_button,
				      flags, reverse));
	x += text_width;

	/* Draw arrow */
	x += UI_LINK_ARROW_MARGIN_H;
	if (!(item->flags & UI_MENU_ITEM_FLAG_NO_ARROW)) {
		arrow_file = reverse ? "ic_dropleft.bmp" : "ic_dropright.bmp";
		VB2_TRY(ui_get_bitmap(arrow_file, NULL, focused, &bitmap));
		VB2_TRY(ui_draw_bitmap(&bitmap, x, y_center,
				       UI_LINK_ARROW_SIZE, UI_LINK_ARROW_SIZE,
				       flags, reverse));
		x += UI_LINK_ARROW_SIZE + UI_LINK_ARROW_MARGIN_H;
	}

	/* Draw button borders */
	if (focused)
		VB2_TRY(ui_draw_rounded_box(x_base, y, width, height,
					    &ui_color_link_border,
					    UI_LINK_BORDER_THICKNESS,
					    UI_BUTTON_BORDER_RADIUS, reverse));

	return VB2_SUCCESS;
}

vb2_error_t ui_draw_desc(const struct ui_desc *desc,
			 const struct ui_state *state,
			 int32_t *y)
{
	int i;
	struct ui_bitmap bitmap;
	const char *locale_code = state->locale->code;
	const int reverse = state->locale->rtl;
	int32_t x;
	const int32_t w = UI_SIZE_AUTO;
	uint32_t flags = PIVOT_H_LEFT | PIVOT_V_TOP;

	x = UI_MARGIN_H;

	for (i = 0; i < desc->count; i++) {
		int32_t h;
		if (i > 0)
			*y += UI_DESC_TEXT_LINE_SPACING;
		VB2_TRY(ui_get_bitmap(desc->files[i], locale_code, 0, &bitmap));
		h = UI_DESC_TEXT_HEIGHT * ui_get_bitmap_num_lines(&bitmap);
		VB2_TRY(ui_draw_bitmap(&bitmap, x, *y, w, h, flags, reverse));
		*y += h;
	}

	return VB2_SUCCESS;
}

static int count_lines(const char *str)
{
	const char *c = str;
	int num_lines;
	if (!str || *c == '\0')
		return 0;

	num_lines = 1;
	while (*c != '\0') {
		if (*c == '\n')
			num_lines++;
		c++;
	}
	return num_lines;
}

vb2_error_t ui_draw_textbox(const char *str, int32_t *y, int32_t min_lines)
{
	vb2_error_t rv = VB2_SUCCESS;
	int32_t x;
	int32_t y_base = *y;
	int32_t max_height = UI_BOX_TEXT_HEIGHT;
	int num_lines;
	int32_t max_content_height, content_width, line_spacing = 0;
	int32_t box_width, box_height;
	char *buf, *end, *line, *ptr;

	/* Copy str to buf since strsep() will modify the string. */
	buf = strdup(str);
	if (!buf) {
		UI_ERROR("Failed to malloc string buffer\n");
		return VB2_ERROR_UI_MEMORY_ALLOC;
	}

	/*
	 * TODO(b/166741235): Replace <TAB> with 8 spaces instead of 1.
	 * This change should be done in log.c, and remove the following lines
	 * here.
	 */
	for (ptr = buf; *ptr != '\0'; ptr++)
		if (*ptr =='\t')
			*ptr = ' ';

	num_lines = MAX(count_lines(buf), min_lines);
	max_content_height = UI_SCALE - UI_BOX_MARGIN_V * 2 -
		UI_BOX_PADDING_V * 2;
	line_spacing = UI_BOX_TEXT_LINE_SPACING * (num_lines - 1);

	if (max_height * num_lines + line_spacing > max_content_height)
		max_height = (max_content_height - line_spacing) / num_lines;

	x = UI_MARGIN_H;
	box_width = UI_SCALE - UI_MARGIN_H * 2;
	content_width = box_width - UI_BOX_PADDING_H * 2;
	box_height = max_height * num_lines + line_spacing +
		UI_BOX_PADDING_V * 2;

	/* Clear printing area. */
	ui_draw_rounded_box(x, *y, box_width, box_height,
			    &ui_color_bg, 0, 0, 0);
	/* Draw the border of a box. */
	ui_draw_rounded_box(x, *y, box_width, box_height, &ui_color_fg,
			    UI_BOX_BORDER_THICKNESS, UI_BOX_BORDER_RADIUS, 0);

	x += UI_BOX_PADDING_H;
	*y += UI_BOX_PADDING_V;

	end = buf;
	while ((line = strsep(&end, "\n"))) {
		vb2_error_t line_rv;
		int32_t line_width;
		int32_t line_height = max_height;
		/* Ensure the text width is no more than box width */
		line_rv = ui_get_text_width(line, line_height, &line_width);
		if (line_rv) {
			/* Save the first error in rv */
			if (rv == VB2_SUCCESS)
				rv = line_rv;
			continue;
		}
		if (line_width > content_width)
			line_height = line_height * content_width / line_width;
		line_rv = ui_draw_text(line, x, *y, line_height,
				       &ui_color_bg, &ui_color_fg,
				       PIVOT_H_LEFT | PIVOT_V_TOP, 0);
		*y += line_height + UI_BOX_TEXT_LINE_SPACING;
		/* Save the first error in rv */
		if (line_rv && rv == VB2_SUCCESS)
			rv = line_rv;
	}

	*y = y_base + box_height;
	free(buf);
	return rv;
}

vb2_error_t ui_get_log_textbox_dimensions(enum vb2_screen screen,
					  const char *locale_code,
					  uint32_t *lines_per_page,
					  uint32_t *chars_per_line)
{
	const struct ui_screen_info *screen_info;
	struct ui_bitmap bitmap;
	int32_t title_height;
	int32_t textbox_height;
	int32_t char_width;

	screen_info = ui_get_screen_info(screen);
	VB2_TRY(ui_get_bitmap(screen_info->title, locale_code, 0, &bitmap));
	title_height = UI_TITLE_TEXT_HEIGHT * ui_get_bitmap_num_lines(&bitmap);

	/* Calculate textbox height by subtracting the height of other items
	   from UI_SCALE. */
	textbox_height = UI_SCALE -
		UI_MARGIN_TOP -
		UI_LANG_BOX_HEIGHT - UI_LANG_MARGIN_BOTTOM -
		title_height - UI_TITLE_MARGIN_BOTTOM -
		/* Page up, page down, back, and power off button */
		(UI_BUTTON_HEIGHT + UI_BUTTON_MARGIN_V) * 4 -
		UI_DESC_MARGIN_BOTTOM -
		UI_FOOTER_MARGIN_TOP - UI_FOOTER_HEIGHT -
		UI_MARGIN_BOTTOM;

	*lines_per_page = (textbox_height - UI_BOX_PADDING_V * 2) /
			  (UI_BOX_TEXT_HEIGHT + UI_BOX_TEXT_LINE_SPACING);

	VB2_TRY(ui_get_text_width("?", UI_BOX_TEXT_HEIGHT, &char_width));

	*chars_per_line = (UI_SCALE - UI_MARGIN_H * 2 - UI_BOX_PADDING_H * 2) /
			  char_width;

	return VB2_SUCCESS;
}

vb2_error_t ui_draw_log_textbox(const char *str, const struct ui_state *state,
				int32_t *y)
{
	uint32_t lines_per_page, chars_per_line;
	VB2_TRY(ui_get_log_textbox_dimensions(state->screen->id,
					      state->locale->code,
					      &lines_per_page,
					      &chars_per_line));
	return ui_draw_textbox(str, y, lines_per_page);
}

static vb2_error_t ui_draw_dev_signed_warning(void)
{
	struct vb2_context *ctx = vboot_get_context();

	/* Dev-mode boots everything anyway, this is only interesting in rec. */
	if (!(ctx->flags & VB2_CONTEXT_RECOVERY_MODE))
		return VB2_SUCCESS;
	if (!vb2api_is_developer_signed(ctx))
		return VB2_SUCCESS;

	const int32_t x = UI_MARGIN_H;
	const int32_t y = UI_MARGIN_TOP + UI_LANG_BOX_HEIGHT +
		UI_LANG_MARGIN_BOTTOM / 2;

	VB2_TRY(ui_draw_text("This firmware is developer-signed. "
			     "MP-signed recovery images will not work!",
			     x, y,
			     UI_BOX_TEXT_HEIGHT,
			     &ui_color_bg, &ui_color_fg,
			     PIVOT_H_LEFT | PIVOT_V_CENTER,
			     0));

	return VB2_SUCCESS;
}

vb2_error_t ui_draw_menu_items(const struct ui_menu *menu,
			       const struct ui_state *state,
			       const struct ui_state *prev_state,
			       int32_t y)
{
	int i;
	const char *locale_code = state->locale->code;
	const int reverse = state->locale->rtl;
	int32_t x;
	int32_t button_width;
	int clear_help;

	/* Primary buttons */
	x = UI_MARGIN_H;
	VB2_TRY(ui_get_button_width(menu, state, &button_width));
	for (i = 0; i < menu->num_items; i++) {
		if (menu->items[i].type != UI_MENU_ITEM_TYPE_PRIMARY)
			continue;
		if (VB2_GET_BIT(state->hidden_item_mask, i))
			continue;
		clear_help = prev_state &&
			     prev_state->selected_item == i &&
			     VB2_GET_BIT(prev_state->disabled_item_mask, i);
		VB2_TRY(ui_draw_button(&menu->items[i],
				       locale_code,
				       x, y,
				       button_width, UI_BUTTON_HEIGHT,
				       reverse, state->selected_item == i,
				       VB2_GET_BIT(state->disabled_item_mask,
						   i),
				       clear_help));
		y += UI_BUTTON_HEIGHT + UI_BUTTON_MARGIN_V;
	}

	/* Secondary (link) buttons */
	x = UI_MARGIN_H - UI_LINK_TEXT_PADDING_LEFT;
	y = UI_SCALE - UI_MARGIN_BOTTOM - UI_FOOTER_HEIGHT -
		UI_FOOTER_MARGIN_TOP - UI_BUTTON_HEIGHT;
	for (i = menu->num_items - 1; i >= 0; i--) {
		if (VB2_GET_BIT(state->hidden_item_mask, i))
			continue;
		if (menu->items[i].type != UI_MENU_ITEM_TYPE_SECONDARY)
			continue;
		VB2_TRY(ui_draw_link(&menu->items[i], locale_code,
				     x, y, UI_BUTTON_HEIGHT, reverse,
				     state->selected_item == i));
		y -= UI_BUTTON_HEIGHT + UI_BUTTON_MARGIN_V;
	}

	return VB2_SUCCESS;
}

vb2_error_t ui_draw_default(const struct ui_state *state,
			    const struct ui_state *prev_state)
{
	const struct ui_screen_info *screen = state->screen;
	const struct ui_menu *menu = &screen->menu;
	const char *locale_code = state->locale->code;
	const int reverse = state->locale->rtl;
	int focused;
	int32_t x, y;
	const int32_t w = UI_SIZE_AUTO;
	int32_t h;
	uint32_t flags = PIVOT_H_LEFT | PIVOT_V_TOP;
	const char *icon_file;
	struct ui_bitmap bitmap;

	if (!prev_state ||
	    prev_state->locale != state->locale ||
	    prev_state->error_code != state->error_code) {
		/*
		 * Clear the whole screen if previous drawing failed, if there
		 * is no previous screen, or if locale changed.
		 */
		clear_screen(&ui_color_bg);
	} else if (prev_state->screen != state->screen) {
		/* Clear everything above the footer for new screen. */
		const int32_t box_height = UI_SCALE - UI_MARGIN_BOTTOM -
			UI_FOOTER_HEIGHT;
		VB2_TRY(ui_draw_box(0, 0, UI_SCALE, box_height,
				    &ui_color_bg, 0));
	}

	/* Warning if we are in recovery and using dev signed keys. */
	if (screen->id != VB2_SCREEN_LANGUAGE_SELECT)
		VB2_TRY(ui_draw_dev_signed_warning());

	/* Language dropdown header */
	if (menu->num_items > 0 &&
	    menu->items[0].type == UI_MENU_ITEM_TYPE_LANGUAGE) {
		focused = state->selected_item == 0;
		if (!prev_state ||
		    prev_state->screen != state->screen ||
		    prev_state->locale != state->locale ||
		    prev_state->error_code != state->error_code ||
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
	     prev_state->locale != state->locale ||
	     prev_state->error_code != state->error_code))
		VB2_TRY(draw_footer(state));

	x = UI_MARGIN_H;
	y = UI_MARGIN_TOP + UI_LANG_BOX_HEIGHT + UI_LANG_MARGIN_BOTTOM;

	/* Icon */
	if (screen->icon != UI_ICON_TYPE_NONE) {
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

		if (screen->icon == UI_ICON_TYPE_STEP) {
			VB2_TRY(ui_draw_step_icons(state, prev_state));
		} else if (icon_file) {
			VB2_TRY(ui_get_bitmap(icon_file, NULL, 0, &bitmap));
			VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, UI_ICON_HEIGHT,
					       flags, reverse));
		}
		y += UI_ICON_HEIGHT + UI_ICON_MARGIN_BOTTOM;
	}

	/* Title */
	if (screen->title) {
		VB2_TRY(ui_get_bitmap(screen->title, locale_code, 0, &bitmap));
		h = UI_TITLE_TEXT_HEIGHT * ui_get_bitmap_num_lines(&bitmap);
		VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, h, flags, reverse));
	} else {
		h = UI_TITLE_TEXT_HEIGHT;
	}
	y += h + UI_TITLE_MARGIN_BOTTOM;

	/* Description */
	if (screen->draw_desc)
		VB2_TRY(screen->draw_desc(state, prev_state, &y));
	else
		VB2_TRY(ui_draw_desc(&screen->desc, state, &y));
	y += UI_DESC_MARGIN_BOTTOM;

	/* Primary and secondary buttons */
	VB2_TRY(ui_draw_menu_items(menu, state, prev_state, y));

	return VB2_SUCCESS;
}

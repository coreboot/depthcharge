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
	int32_t x, y, text_height;
	int32_t col2_width;
	const int32_t w = UI_SIZE_AUTO;
	const int32_t footer_y = UI_SCALE - UI_MARGIN_BOTTOM - UI_FOOTER_HEIGHT;
	const int32_t footer_height = UI_FOOTER_HEIGHT;
	struct ui_bitmap bitmap;

	/* Column 1 */
	x = UI_MARGIN_LEFT;
	y = footer_y;
	VB2_TRY(ui_get_bitmap("qr_code.bmp", &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, footer_height, footer_height,
			       flags, reverse));
	x += footer_height + UI_FOOTER_COL1_MARGIN_RIGHT;

	/* Column 2: 4 lines of text */
	y = footer_y;
	text_height = (footer_height - UI_FOOTER_COL2_PARA_SPACING) / 4;
	VB2_TRY(ui_get_localized_bitmap("model.bmp", locale_code, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, text_height, flags, reverse));
	y += text_height;
	VB2_TRY(ui_draw_text(hwid, x, y, text_height, flags, UI_CHAR_STYLE_DARK,
			     reverse));
	y += text_height + UI_FOOTER_COL2_PARA_SPACING;
	VB2_TRY(ui_get_localized_bitmap("help_center.bmp", locale_code,
					&bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, text_height, flags, reverse));
	y += text_height;
	VB2_TRY(ui_get_localized_bitmap("rec_url.bmp", locale_code, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, text_height, flags, reverse));
	VB2_TRY(ui_get_bitmap_width(&bitmap, text_height, &col2_width));
	x += col2_width + UI_FOOTER_COL2_MARGIN_RIGHT;

	/* Separator */
	VB2_TRY(ui_draw_box(x, footer_y, UI_SIZE_MIN, footer_height,
			    &ui_color_separator, reverse));

	/* Column 3: 2 lines of text */
	int32_t icon_width;
	const int32_t icon_height = UI_FOOTER_COL3_ICON_HEIGHT;
	x += UI_FOOTER_COL3_MARGIN_LEFT;
	y = footer_y;
	text_height = (footer_height - UI_FOOTER_COL3_LINE_SPACING -
		       UI_FOOTER_COL3_ICON_MARGIN_TOP - icon_height) / 2;
	VB2_TRY(ui_get_localized_bitmap("to_navigate.bmp", locale_code,
					&bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, text_height, flags, reverse));
	y += text_height + UI_FOOTER_COL3_LINE_SPACING;

	VB2_TRY(ui_get_localized_bitmap("navigate.bmp", locale_code, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, text_height, flags, reverse));
	y += text_height + UI_FOOTER_COL3_ICON_MARGIN_TOP;

	const char *const icon_files[] = {
		"nav-key_enter.bmp",
		"nav-key_up.bmp",
		"nav-key_down.bmp",
	};

	for (int i = 0; i < ARRAY_SIZE(icon_files); i++) {
		VB2_TRY(ui_get_bitmap(icon_files[i], &bitmap));
		VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, icon_height,
				       flags, reverse));
		VB2_TRY(ui_get_bitmap_width(&bitmap, icon_height, &icon_width));
		x += icon_width + UI_FOOTER_COL3_ICON_SPACING;
	}

	return VB2_SUCCESS;
}

vb2_error_t ui_draw_content(const struct ui_state *state,
			    const struct ui_state *prev_state,
			    enum ui_icon_type icon, const char *title,
			    const char *const desc[], size_t desc_size)
{
	int i;
	const char *locale_code = state->locale->code;
	const int reverse = state->locale->rtl;
	int32_t x, y, h;
	const int32_t w = UI_SIZE_AUTO;
	uint32_t flags = PIVOT_H_LEFT | PIVOT_V_TOP;
	const char *icon_file;
	struct ui_bitmap bitmap;

	if (!prev_state) {
		/*
		 * Clear the whole screen if previous drawing failed or there
		 * is no previous screen.
		 */
		clear_screen(&ui_color_bg);
	} else if (prev_state->screen != state->screen ||
		   prev_state->locale != state->locale) {
		/*
		 * Clear everything above the footer for new screen or if locale
		 * changed.
		 */
		const int32_t box_height = UI_SCALE - UI_MARGIN_BOTTOM -
			UI_FOOTER_HEIGHT;
		VB2_TRY(ui_draw_box(0, 0, UI_SCALE, box_height,
				    &ui_color_bg, 0));
	}

	/* TODO(yupingso): Draw language item */

	/*
	 * Draw the footer if previous screen doesn't have a footer, or if
	 * locale changed.
	 */
	if ((!prev_state ||
	     prev_state->screen == VB2_SCREEN_BLANK ||
	     prev_state->screen == VB2_SCREEN_FIRMWARE_SYNC ||
	     prev_state->locale != state->locale) &&
	    state->screen != VB2_SCREEN_FIRMWARE_SYNC)
		VB2_TRY(draw_footer(state));

	/* Icon */
	switch (icon) {
	case UI_ICON_TYPE_INFO:
		icon_file = "ic_info.bmp";
		break;
	default:
		icon_file = NULL;
		break;
	}

	x = UI_MARGIN_LEFT;
	y = UI_MARGIN_TOP + UI_LANG_BOX_HEIGHT + UI_LANG_MARGIN_BOTTOM;
	if (icon_file) {
		VB2_TRY(ui_get_bitmap(icon_file, &bitmap));
		VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, UI_ICON_HEIGHT,
				       flags, reverse));
	}
	y += UI_ICON_HEIGHT + UI_ICON_MARGIN_BOTTOM;

	/* Title */
	VB2_TRY(ui_get_localized_bitmap(title, locale_code, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, UI_TITLE_TEXT_HEIGHT,
			       flags, reverse));
	y += UI_TITLE_TEXT_HEIGHT + UI_TITLE_MARGIN_BOTTOM;

	/* Description */
	h = UI_DESC_TEXT_HEIGHT;
	for (i = 0; i < desc_size; i++) {
		VB2_TRY(ui_get_localized_bitmap(desc[i], locale_code, &bitmap));
		VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, h, flags, reverse));
		y += h + UI_DESC_TEXT_LINE_SPACING;
	}

	return VB2_SUCCESS;
}

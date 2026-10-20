// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC
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
#include <lp_vboot.h>

#include "base/vpd_util.h"
#include "boot/android_vpd.h"
#include "vboot/ui.h"

/*
 * Get the height of a text block.
 *
 * The text bitmap is vertically centered in the text block. The first line of
 * the text takes up line_height, and each additional line takes up
 * text_height, because the lines in the bitmap are text_height apart.
 *
 * @param bitmap	Text bitmap.
 * @param text_height	Height of each line of the bitmap.
 * @param line_height	Line height of the text in the design.
 *
 * @return The height of the text block.
 */
static int32_t ui_get_text_block_height(const struct ui_bitmap *bitmap,
					int32_t text_height,
					int32_t line_height)
{
	return text_height * ui_get_bitmap_num_lines(bitmap) +
		line_height - text_height;
}

static const char *get_device_from_vpd(void)
{
	static char device[ANDROID_VPD_MAX_BUFFER_SIZE];

	if (!vpd_gets(ANDROID_VPD_KEY_DEVICE, device, sizeof(device))) {
		printf("%s: VPD key %s not found\n", __func__,
		       ANDROID_VPD_KEY_DEVICE);
		return NULL;
	}

	return device;
}

static const char *get_device_from_hwid(void)
{
	static char hwid[VB2_GBB_HWID_MAX_SIZE] = {0};
	uint32_t hwid_size = sizeof(hwid);

	if (vb2api_gbb_read_hwid(vboot_get_context(), hwid, &hwid_size)) {
		printf("%s: Failed to read HWID\n", __func__);
		return NULL;
	}

	/* Truncate everything after the first whitespace. */
	char *p = strchr(hwid, ' ');
	if (p)
		*p = '\0';
	return hwid;
}

/* Keys for navigating the menu */
enum nav_key {
	NAV_KEY_UP,
	NAV_KEY_DOWN,
	NAV_KEY_SELECT,
};

/* Keycap of a navigation key */
struct keycap {
	/* Generic bitmap filename of the icon or the text label. */
	const char *file;
	/* Whether the bitmap is a text label instead of an icon. */
	bool is_text;
};

/*
 * Keycaps of the navigation keys, indexed by enum nav_key. On detachables, the
 * volume buttons and the power button are used for navigation instead of the
 * arrow keys and the Enter key.
 */
static const struct keycap keyboard_keycaps[] = {
	[NAV_KEY_UP] = { .file = "ic_arrow_up.bmp" },
	[NAV_KEY_DOWN] = { .file = "ic_arrow_down.bmp" },
	[NAV_KEY_SELECT] = { .file = "key_enter.bmp", .is_text = true },
};

static const struct keycap detachable_keycaps[] = {
	[NAV_KEY_UP] = { .file = "ic_volume_up.bmp" },
	[NAV_KEY_DOWN] = { .file = "ic_volume_down.bmp" },
	[NAV_KEY_SELECT] = { .file = "ic_power_settings_new.bmp" },
};

static const struct keycap *get_keycap(enum nav_key key)
{
	if (CONFIG(DETACHABLE))
		return &detachable_keycaps[key];
	return &keyboard_keycaps[key];
}

/*
 * Get the size of the keycap of a navigation key.
 *
 * @param key		Navigation key.
 * @param width		Width of the keycap to be returned.
 * @param height	Height of the keycap to be returned.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
static vb2_error_t get_keycap_size(enum nav_key key, int32_t *width,
				   int32_t *height)
{
	const struct keycap *keycap = get_keycap(key);
	struct ui_bitmap bitmap;

	if (!keycap->is_text) {
		*width = UI_KEYCAP_SIZE;
		*height = UI_KEYCAP_SIZE;
		return VB2_SUCCESS;
	}

	VB2_TRY(ui_get_bitmap(keycap->file, NULL, 0, &bitmap));
	VB2_TRY(ui_get_bitmap_width(&bitmap, UI_KEYCAP_TEXT_HEIGHT, width));
	*width += UI_KEYCAP_PADDING_H * 2;
	*height = UI_KEYCAP_TEXT_BOX_HEIGHT;
	return VB2_SUCCESS;
}

/*
 * Draw the keycap of a navigation key, vertically centered at y_center.
 *
 * The keycap is a rounded box with an icon or a text label inside.
 *
 * @param key		Navigation key.
 * @param x		x-coordinate of the left edge.
 * @param y_center	y-coordinate of the vertical center.
 * @param reverse	Whether to reverse the x-coordinate relative to the
 *			canvas.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
static vb2_error_t draw_keycap(enum nav_key key, int32_t x, int32_t y_center,
			       int reverse)
{
	const struct keycap *keycap = get_keycap(key);
	struct ui_bitmap bitmap;
	int32_t width, height;

	VB2_TRY(get_keycap_size(key, &width, &height));
	VB2_TRY(ui_draw_rounded_box(x, y_center - height / 2, width, height,
				    &ui_color_border,
				    UI_KEYCAP_BORDER_THICKNESS,
				    UI_KEYCAP_BORDER_RADIUS, reverse));
	VB2_TRY(ui_get_bitmap(keycap->file, NULL, 0, &bitmap));
	if (keycap->is_text)
		return ui_draw_mapped_bitmap(&bitmap, x + UI_KEYCAP_PADDING_H,
					     y_center, UI_SIZE_AUTO,
					     UI_KEYCAP_TEXT_HEIGHT,
					     &ui_color_bg, &ui_color_keycap_fg,
					     PIVOT_H_LEFT | PIVOT_V_CENTER,
					     reverse);
	return ui_draw_mapped_bitmap(&bitmap, x + width / 2, y_center,
				     UI_KEYCAP_ICON_SIZE, UI_KEYCAP_ICON_SIZE,
				     &ui_color_bg, &ui_color_keycap_fg,
				     PIVOT_H_CENTER | PIVOT_V_CENTER, reverse);
}

/*
 * Get the height of a footer text block, including the spacing between the
 * text and the line box.
 */
static int32_t get_footer_text_block_height(const struct ui_bitmap *bitmap)
{
	return ui_get_text_block_height(bitmap, UI_FOOTER_TEXT_HEIGHT,
					UI_FOOTER_TEXT_LINE_HEIGHT);
}

/*
 * Draw a footer text bitmap, vertically centered at y_center.
 */
static vb2_error_t draw_footer_text(const struct ui_bitmap *bitmap,
				    int32_t x, int32_t y_center, int reverse)
{
	const int32_t h =
		UI_FOOTER_TEXT_HEIGHT * ui_get_bitmap_num_lines(bitmap);

	return ui_draw_bitmap(bitmap, x, y_center, UI_SIZE_AUTO, h,
			      PIVOT_H_LEFT | PIVOT_V_CENTER, reverse);
}

/*
 * Get the height of a row in the left column of the footer, including the
 * vertical padding. The row consists of keycaps and a text block.
 */
static int32_t get_footer_row_height(int32_t keycap_height,
				     const struct ui_bitmap *bitmap)
{
	return MAX(keycap_height, get_footer_text_block_height(bitmap)) +
		UI_FOOTER_ROW_PADDING_V * 2;
}

/*
 * Get the height of the left column of the footer.
 *
 * When the text wraps, the rows may take up the spacing between them, so that
 * the column doesn't grow for 2-line text.
 *
 * @param locale_code	Language code of locale.
 * @param height	Height of the left column to be returned.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
static vb2_error_t get_footer_left_height(const char *locale_code,
					  int32_t *height)
{
	struct ui_bitmap bitmap;
	int32_t keycap_width, keycap1_height, keycap2_height;
	int32_t row1_height, row2_height;

	VB2_TRY(ui_get_bitmap("navigate0.bmp", locale_code, 0, &bitmap));
	VB2_TRY(get_keycap_size(NAV_KEY_DOWN, &keycap_width,
				&keycap1_height));
	row1_height = get_footer_row_height(keycap1_height, &bitmap);
	VB2_TRY(ui_get_bitmap("navigate1.bmp", locale_code, 0, &bitmap));
	VB2_TRY(get_keycap_size(NAV_KEY_SELECT, &keycap_width,
				&keycap2_height));
	row2_height = get_footer_row_height(keycap2_height, &bitmap);

	*height = MAX(row1_height + row2_height,
		      keycap1_height + keycap2_height +
		      UI_FOOTER_ROW_PADDING_V * 4 + UI_FOOTER_ROW_SPACING);
	return VB2_SUCCESS;
}

/*
 * Get the y-coordinate of the top of the footer.
 *
 * The footer is anchored to the bottom of the screen, and its height is
 * determined by the taller column. Therefore, the footer grows upward when the
 * text wraps to too many lines.
 *
 * @param locale_code	Language code of locale.
 * @param y		y-coordinate of the top of the footer to be returned.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
static vb2_error_t get_footer_y(const char *locale_code, int32_t *y)
{
	struct ui_bitmap bitmap;
	int32_t left_height, right_height;

	/* Left column: keyboard navigation instructions */
	VB2_TRY(get_footer_left_height(locale_code, &left_height));

	/* Right column: QR code, device name and help center URL */
	VB2_TRY(ui_get_bitmap("device.bmp", locale_code, 0, &bitmap));
	right_height = get_footer_text_block_height(&bitmap) +
		UI_FOOTER_TEXT_LINE_HEIGHT + UI_FOOTER_PARA_SPACING;
	VB2_TRY(ui_get_bitmap("help_center.bmp", locale_code, 0, &bitmap));
	right_height += get_footer_text_block_height(&bitmap) +
		UI_FOOTER_TEXT_LINE_HEIGHT;
	right_height = MAX(right_height, UI_FOOTER_QR_BOX_SIZE);

	*y = UI_SCALE - UI_FOOTER_PADDING_BOTTOM -
		MAX(left_height, right_height) - UI_FOOTER_PADDING_TOP -
		UI_FOOTER_DIVIDER_THICKNESS;
	return VB2_SUCCESS;
}

static vb2_error_t draw_footer(const struct ui_state *state)
{
	const char *locale_code = state->locale->code;
	const int reverse = state->locale->rtl;
	const int32_t line_height = UI_FOOTER_TEXT_LINE_HEIGHT;
	int32_t footer_y, content_y;
	int32_t x, y, h, row_height, y_center, keycap_width, keycap_height;
	struct ui_bitmap bitmap;

	/* Get device name. */
	static const char *device;
	if (!device) {
		device = get_device_from_vpd();
		if (!device)
			device = get_device_from_hwid();
		if (!device)
			device = "NOT FOUND";
	}

	VB2_TRY(get_footer_y(locale_code, &footer_y));
	content_y = footer_y + UI_FOOTER_DIVIDER_THICKNESS +
		UI_FOOTER_PADDING_TOP;

	/* Divider */
	VB2_TRY(ui_draw_box(UI_MARGIN_H, footer_y, UI_SCALE - UI_MARGIN_H * 2,
			    UI_FOOTER_DIVIDER_THICKNESS, &ui_color_border,
			    reverse));

	/*
	 * TODO(b/551743258): Draw the column headers ("How to navigate with
	 * keyboard" and "More details") once the strings are translated.
	 */

	/* Left column: keyboard navigation instructions */
	x = UI_MARGIN_H;
	y = content_y;

	/* Row 1: down and up keys, aligned to the top of the footer */
	VB2_TRY(ui_get_bitmap("navigate0.bmp", locale_code, 0, &bitmap));
	VB2_TRY(get_keycap_size(NAV_KEY_DOWN, &keycap_width, &keycap_height));
	row_height = get_footer_row_height(keycap_height, &bitmap);
	y_center = y + row_height / 2;
	VB2_TRY(draw_keycap(NAV_KEY_DOWN, x, y_center, reverse));
	x += keycap_width + UI_KEYCAP_SPACING;
	VB2_TRY(draw_keycap(NAV_KEY_UP, x, y_center, reverse));
	x += keycap_width + UI_KEYCAP_MARGIN_R;
	VB2_TRY(draw_footer_text(&bitmap, x, y_center, reverse));

	/*
	 * Row 2: select key, aligned to the bottom of the footer, so that both
	 * columns end at the same line.
	 */
	x = UI_MARGIN_H;
	VB2_TRY(ui_get_bitmap("navigate1.bmp", locale_code, 0, &bitmap));
	VB2_TRY(get_keycap_size(NAV_KEY_SELECT, &keycap_width,
				&keycap_height));
	row_height = get_footer_row_height(keycap_height, &bitmap);
	y = UI_SCALE - UI_FOOTER_PADDING_BOTTOM - row_height;
	y_center = y + row_height / 2;
	VB2_TRY(draw_keycap(NAV_KEY_SELECT, x, y_center, reverse));
	x += keycap_width + UI_KEYCAP_MARGIN_R;
	VB2_TRY(draw_footer_text(&bitmap, x, y_center, reverse));

	/* Right column: QR code, device name and help center URL */
	x = UI_SCALE / 2 + UI_FOOTER_COL_PADDING_INNER;
	y = content_y;

	/*
	 * The QR code bitmap is white on black, without the quiet zone.
	 * Recolor it, and use the padding of the box as the quiet zone.
	 */
	const int32_t box_size = UI_FOOTER_QR_BOX_SIZE;
	const int32_t qr_size = box_size - UI_FOOTER_QR_BOX_PADDING * 2;
	VB2_TRY(ui_draw_rounded_box(x, y, box_size, box_size, &ui_color_qr_bg,
				    0, UI_FOOTER_QR_BOX_RADIUS, reverse));
	VB2_TRY(ui_get_bitmap("qr_rec.bmp", NULL, 0, &bitmap));
	VB2_TRY(ui_draw_mapped_bitmap(&bitmap, x + UI_FOOTER_QR_BOX_PADDING,
				      y + UI_FOOTER_QR_BOX_PADDING,
				      qr_size, qr_size,
				      &ui_color_qr_bg, &ui_color_qr_fg,
				      PIVOT_H_LEFT | PIVOT_V_TOP, reverse));
	x += box_size + UI_FOOTER_QR_BOX_MARGIN_R;

	/* Device name */
	VB2_TRY(ui_get_bitmap("device.bmp", locale_code, 0, &bitmap));
	h = get_footer_text_block_height(&bitmap);
	VB2_TRY(draw_footer_text(&bitmap, x, y + h / 2, reverse));
	y += h;
	VB2_TRY(ui_draw_text(device, x, y + line_height / 2,
			     UI_FOOTER_DEVICE_TEXT_HEIGHT,
			     &ui_color_bg, &ui_color_footer_fg,
			     PIVOT_H_LEFT | PIVOT_V_CENTER, reverse));
	y += line_height + UI_FOOTER_PARA_SPACING;

	/* Help center URL */
	VB2_TRY(ui_get_bitmap("help_center.bmp", locale_code, 0, &bitmap));
	h = get_footer_text_block_height(&bitmap);
	VB2_TRY(draw_footer_text(&bitmap, x, y + h / 2, reverse));
	y += h;
	VB2_TRY(ui_get_bitmap("rec_url.bmp", NULL, 0, &bitmap));
	VB2_TRY(draw_footer_text(&bitmap, x, y + line_height / 2, reverse));

	return VB2_SUCCESS;
}

/*
 * Navigation tips in the navigation bar of the full view layout.
 *
 * TODO(b/551743258): Show "Page down / up" next to the down and up keycaps,
 * and "Exit this view" next to the select keycap as in the design.
 */
static const struct {
	enum nav_key key;
	const char *file;
} navigation_tips[] = {
	{ NAV_KEY_DOWN, "btn_page_down.bmp" },
	{ NAV_KEY_UP, "btn_page_up.bmp" },
	{ NAV_KEY_SELECT, "btn_back.bmp" },
};

/*
 * Get the text bitmap of a navigation tip. While a diagnostic test is
 * running, the select key cancels the test.
 */
static const char *get_navigation_tip_file(const struct ui_state *state,
					   int i)
{
	if (navigation_tips[i].key == NAV_KEY_SELECT &&
	    state->test_state == UI_TEST_STATE_RUNNING)
		return "btn_cancel.bmp";
	return navigation_tips[i].file;
}

vb2_error_t ui_get_navigation_bar_width(const struct ui_state *state,
					int32_t *width)
{
	const char *locale_code = state->locale->code;
	struct ui_bitmap bitmap;
	int32_t keycap_width, keycap_height, text_width;

	*width = 0;
	for (int i = 0; i < ARRAY_SIZE(navigation_tips); i++) {
		if (i > 0)
			*width += UI_NAVIGATION_BAR_ITEM_SPACING;
		VB2_TRY(get_keycap_size(navigation_tips[i].key, &keycap_width,
					&keycap_height));
		VB2_TRY(ui_get_bitmap(get_navigation_tip_file(state, i),
				      locale_code, 0, &bitmap));
		VB2_TRY(ui_get_bitmap_width(&bitmap, UI_BUTTON_TEXT_HEIGHT,
					    &text_width));
		*width += keycap_width + UI_KEYCAP_MARGIN_R + text_width;
	}

	return VB2_SUCCESS;
}

/*
 * Draw the navigation bar of the full view layout.
 *
 * @param state		UI state.
 * @param y		y-coordinate of the top of the navigation bar.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
static vb2_error_t draw_navigation_bar(const struct ui_state *state,
				       int32_t y)
{
	const char *locale_code = state->locale->code;
	const int reverse = state->locale->rtl;
	const int32_t y_center = y + UI_NAVIGATION_BAR_HEIGHT / 2;
	const int32_t text_height = UI_BUTTON_TEXT_HEIGHT;
	int32_t x = UI_MARGIN_H;
	int32_t keycap_width, keycap_height, text_width;
	struct ui_bitmap bitmap;

	for (int i = 0; i < ARRAY_SIZE(navigation_tips); i++) {
		if (i > 0)
			x += UI_NAVIGATION_BAR_ITEM_SPACING;
		VB2_TRY(get_keycap_size(navigation_tips[i].key, &keycap_width,
					&keycap_height));
		VB2_TRY(draw_keycap(navigation_tips[i].key, x, y_center,
				    reverse));
		x += keycap_width + UI_KEYCAP_MARGIN_R;
		VB2_TRY(ui_get_bitmap(get_navigation_tip_file(state, i),
				      locale_code, 0, &bitmap));
		VB2_TRY(ui_draw_mapped_bitmap(&bitmap, x, y_center,
					      UI_SIZE_AUTO, text_height,
					      &ui_color_bg, &ui_color_fg,
					      PIVOT_H_LEFT | PIVOT_V_CENTER,
					      reverse));
		VB2_TRY(ui_get_bitmap_width(&bitmap, text_height, &text_width));
		x += text_width;
	}

	/* Clear the rest of the bar, in case the previous tips were wider. */
	VB2_TRY(ui_draw_box(x, y, UI_SCALE - UI_MARGIN_H - x,
			    UI_NAVIGATION_BAR_HEIGHT, &ui_color_bg, reverse));

	return VB2_SUCCESS;
}

static vb2_error_t get_item_file(const struct ui_menu_item *item,
				 const struct ui_state *state,
				 const char **out_file)
{
	if (item->get_file) {
		*out_file = item->get_file(state);
		if (!*out_file) {
			UI_ERROR("Menu item <%s>: get_file() returned NULL\n",
				 item->name ? item->name : "unnamed");
			return VB2_ERROR_UI_DRAW_FAILURE;
		}
	} else {
		*out_file = item->file;
	}
	return VB2_SUCCESS;
}

static const char *get_item_locale_code(const struct ui_menu_item *item,
					const struct ui_state *state)
{
	if (item->flags & UI_MENU_ITEM_FLAG_GENERIC_ARCHIVE)
		return NULL;
	return state->locale->code;
}

static bool is_menu_item_focused(const struct ui_state *state, size_t item_index)
{
	if (!state || state->menu_state.focused_item != item_index)
		return false;

	if (state->is_sub_menu_active)
		return state->sub_menu_state.trigger_focused;

	return true;
}

static bool is_dropdown_expanded(const struct ui_state *state,
				 size_t item_index)
{
	return state && state->is_sub_menu_active &&
	       state->menu_state.focused_item == item_index;
}

static vb2_error_t draw_focus_ring(int32_t x, int32_t y,
				   int32_t width, int32_t height, int reverse)
{
	const uint32_t radius = height / 2;
	const uint32_t t = UI_BUTTON_FOCUS_RING_THICKNESS;

	VB2_TRY(ui_draw_rounded_box(x, y, width, height,
				    &ui_color_button_focus_ring, t,
				    radius, reverse));
	return ui_draw_rounded_box(x + t, y + t,
				   width - t * 2, height - t * 2,
				   &ui_color_bg, t,
				   radius - t, reverse);
}

vb2_error_t ui_get_h1_button_width(const struct ui_menu_item *item,
				   const struct ui_state *state,
				   int32_t *button_width)
{
	const char *file;
	struct ui_bitmap bitmap;
	int32_t text_width;

	VB2_TRY(get_item_file(item, state, &file));
	if (item->get_width) {
		VB2_TRY(item->get_width(state, &text_width));
	} else if (file) {
		VB2_TRY(ui_get_bitmap(file, state->locale->code, 0, &bitmap));
		VB2_TRY(ui_get_bitmap_width(&bitmap, UI_BUTTON_TEXT_HEIGHT,
					    &text_width));
	} else if (item->name) {
		VB2_TRY(ui_get_text_width(item->name, UI_BUTTON_TEXT_HEIGHT,
					  &text_width));
	} else {
		UI_ERROR("Menu item: no .file or .name\n");
		return VB2_ERROR_UI_DRAW_FAILURE;
	}

	*button_width = text_width + UI_BUTTON_PADDING_H * 2;
	return VB2_SUCCESS;
}

vb2_error_t ui_draw_h1_button(const struct ui_menu_item *item,
			      const struct ui_state *state,
			      int32_t x, int32_t y, int32_t height,
			      int focused, int disabled)
{
	struct ui_bitmap bitmap;
	int32_t width;
	const uint32_t flags = PIVOT_H_CENTER | PIVOT_V_CENTER;
	const struct rgb_color *bg_color, *fg_color;
	const char *file;
	const char *locale_code = state->locale->code;
	const int reverse = state->locale->rtl;

	VB2_TRY(get_item_file(item, state, &file));
	VB2_TRY(ui_get_h1_button_width(item, state, &width));
	const int32_t x_center = x + width / 2;
	const int32_t y_center = y + height / 2;

	/* Set button styles */
	if (disabled) {
		bg_color = &ui_color_button_disabled_bg;
		fg_color = &ui_color_button_disabled_fg;
	} else {
		bg_color = &ui_color_button;
		fg_color = &ui_color_button_fg;
	}

	/* Fill pill button area */
	VB2_TRY(ui_draw_rounded_box(x, y, width, height, bg_color,
				    0, height / 2, reverse));

	if (focused)
		VB2_TRY(draw_focus_ring(x, y, width, height, reverse));

	/* Draw button text */
	if (file) {
		VB2_TRY(ui_get_bitmap(file, locale_code, 0, &bitmap));
		VB2_TRY(ui_draw_mapped_bitmap(&bitmap, x_center, y_center,
					      UI_SIZE_AUTO,
					      UI_BUTTON_TEXT_HEIGHT,
					      bg_color, fg_color,
					      flags, reverse));
	} else if (item->name) {
		VB2_TRY(ui_draw_text(item->name, x_center, y_center,
				     UI_BUTTON_TEXT_HEIGHT,
				     bg_color, fg_color,
				     flags, reverse));
	} else {
		UI_ERROR("No button image filename or text\n");
		return VB2_ERROR_UI_DRAW_FAILURE;
	}

	return VB2_SUCCESS;
}

/*
 * Draw an H2 dropdown trigger button.
 *
 * @param item		Menu item.
 * @param state		UI state.
 * @param x		x-coordinate of the top-left corner.
 * @param y		y-coordinate of the top-left corner.
 * @param height	Height of the box.
 * @param focused	1 for focused and 0 for non-focused.
 * @param expanded	Whether the sub-menu of the dropdown is expanded.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
static vb2_error_t ui_draw_h2_dropdown(const struct ui_menu_item *item,
				       const struct ui_state *state,
				       int32_t x, int32_t y, int32_t height,
				       int focused, int expanded)
{
	struct ui_bitmap bitmap;
	int32_t text_width, width;
	const int32_t x_base = x;
	const int32_t y_center = y + height / 2;
	const uint32_t flags = PIVOT_H_LEFT | PIVOT_V_CENTER;
	const char *file;
	const char *locale_code = get_item_locale_code(item, state);
	const int reverse = state->locale->rtl;

	VB2_TRY(get_item_file(item, state, &file));
	if (!file) {
		UI_ERROR("No dropdown image filename\n");
		return VB2_ERROR_UI_DRAW_FAILURE;
	}

	/* Calculate text width */
	VB2_TRY(ui_get_bitmap(file, locale_code, 0, &bitmap));
	VB2_TRY(ui_get_bitmap_width(&bitmap, UI_BUTTON_TEXT_HEIGHT,
				    &text_width));

	width = UI_BUTTON_PADDING_H + text_width +
		UI_DROPDOWN_ARROW_MARGIN_H + UI_DROPDOWN_ARROW_SIZE +
		UI_BUTTON_PADDING_H;
	if (item->icon_file)
		width += UI_DROPDOWN_ICON_SIZE + UI_DROPDOWN_ARROW_MARGIN_H;
	if (item->type == UI_MENU_ITEM_TYPE_LANGUAGE)
		width = MAX(width, UI_LANG_BUTTON_MIN_WIDTH);

	const struct rgb_color *bg_color = &ui_color_h2_button_bg;
	const struct rgb_color *fg_color = &ui_color_h2_button_fg;

	/* Fill pill button area */
	VB2_TRY(ui_draw_rounded_box(x_base, y, width, height,
				    bg_color, 0,
				    height / 2, reverse));

	x += UI_BUTTON_PADDING_H;

	/* Draw optional icon */
	if (item->icon_file) {
		int32_t icon_size = UI_DROPDOWN_ICON_SIZE;
		struct ui_bitmap icon_bmp;
		VB2_TRY(ui_get_bitmap(item->icon_file, NULL, 0, &icon_bmp));
		VB2_TRY(ui_draw_mapped_bitmap(&icon_bmp, x, y_center,
					      icon_size, icon_size,
					      bg_color, fg_color,
					      flags, reverse));
		x += icon_size + UI_DROPDOWN_ARROW_MARGIN_H;
	}

	/* Draw button text */
	VB2_TRY(ui_draw_mapped_bitmap(&bitmap, x, y_center,
				      UI_SIZE_AUTO,
				      UI_BUTTON_TEXT_HEIGHT,
				      bg_color,
				      fg_color,
				      flags, reverse));

	/* Draw dropdown arrow */
	x = x_base + width - UI_BUTTON_PADDING_H - UI_DROPDOWN_ARROW_SIZE;
	file = expanded ? "ic_arrow_up.bmp" : "ic_arrow_down.bmp";
	VB2_TRY(ui_get_bitmap(file, NULL, 0, &bitmap));
	VB2_TRY(ui_draw_mapped_bitmap(&bitmap, x, y_center,
				      UI_DROPDOWN_ARROW_SIZE,
				      UI_DROPDOWN_ARROW_SIZE,
				      bg_color,
				      fg_color,
				      flags, reverse));

	/* Draw focus ring */
	if (focused)
		VB2_TRY(draw_focus_ring(x_base, y, width, height, reverse));

	return VB2_SUCCESS;
}

/*
 * Draw an H3 button.
 *
 * @param item		Menu item.
 * @param state		UI state.
 * @param x		x-coordinate of the top-left corner.
 * @param y		y-coordinate of the top-left corner.
 * @param height	Height of the box.
 * @param focused	1 for focused and 0 for non-focused.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
static vb2_error_t ui_draw_h3_button(const struct ui_menu_item *item,
				     const struct ui_state *state,
				     int32_t x, int32_t y, int32_t height,
				     int focused)
{
	struct ui_bitmap bitmap;
	int32_t text_width, width;
	const int32_t x_base = x;
	const int32_t y_center = y + height / 2;
	const uint32_t flags = PIVOT_H_LEFT | PIVOT_V_CENTER;
	const struct rgb_color *bg_color = &ui_color_bg;
	const struct rgb_color *fg_color = &ui_color_button;
	const struct rgb_color *border_color = focused ?
		&ui_color_button_focus_ring : &ui_color_h3_button_border;
	const char *file;
	const char *locale_code = state->locale->code;
	const int reverse = state->locale->rtl;

	VB2_TRY(get_item_file(item, state, &file));
	if (!file) {
		UI_ERROR("No H3 button image filename\n");
		return VB2_ERROR_UI_DRAW_FAILURE;
	}

	/* Get button width */
	VB2_TRY(ui_get_bitmap(file, locale_code, 0, &bitmap));
	VB2_TRY(ui_get_bitmap_width(&bitmap, UI_BUTTON_TEXT_HEIGHT,
				    &text_width));
	width = UI_BUTTON_PADDING_H * 2 + text_width;
	if (item->icon_file)
		width += UI_H3_BUTTON_ICON_SIZE + UI_H3_BUTTON_ICON_MARGIN_R;

	/* Clear button area */
	VB2_TRY(ui_draw_rounded_box(x_base, y, width, height,
				    bg_color, 0, height / 2,
				    reverse));

	/* Draw button icon */
	x += UI_BUTTON_PADDING_H;
	if (item->icon_file) {
		VB2_TRY(ui_get_bitmap(item->icon_file, NULL, 0, &bitmap));
		VB2_TRY(ui_draw_mapped_bitmap(&bitmap, x, y_center,
					      UI_H3_BUTTON_ICON_SIZE,
					      UI_H3_BUTTON_ICON_SIZE,
					      bg_color, fg_color,
					      flags, reverse));
		x += UI_H3_BUTTON_ICON_SIZE + UI_H3_BUTTON_ICON_MARGIN_R;
	}

	/* Draw button text */
	VB2_TRY(ui_get_bitmap(file, locale_code, 0, &bitmap));
	VB2_TRY(ui_draw_mapped_bitmap(&bitmap, x, y_center,
				      UI_SIZE_AUTO, UI_BUTTON_TEXT_HEIGHT,
				      bg_color, fg_color,
				      flags, reverse));

	/* Draw button border */
	VB2_TRY(ui_draw_rounded_box(x_base, y, width, height,
				    border_color,
				    UI_H3_BUTTON_BORDER_THICKNESS,
				    height / 2, reverse));

	return VB2_SUCCESS;
}

int32_t ui_get_desc_height(const struct ui_bitmap *bitmap)
{
	return ui_get_text_block_height(bitmap, UI_DESC_TEXT_HEIGHT,
					UI_DESC_TEXT_LINE_HEIGHT);
}

vb2_error_t ui_draw_desc_bitmap(const struct ui_bitmap *bitmap, int32_t y,
				int reverse)
{
	const int32_t h =
		UI_DESC_TEXT_HEIGHT * ui_get_bitmap_num_lines(bitmap);

	return ui_draw_bitmap(bitmap, UI_MARGIN_H,
			      y + ui_get_desc_height(bitmap) / 2,
			      UI_SIZE_AUTO, h, PIVOT_H_LEFT | PIVOT_V_CENTER,
			      reverse);
}

vb2_error_t ui_draw_desc(const struct ui_desc *desc,
			 const struct ui_state *state,
			 int32_t *y)
{
	int i;
	struct ui_bitmap bitmap;
	const char *locale_code = state->locale->code;
	const int reverse = state->locale->rtl;

	for (i = 0; i < desc->count; i++) {
		if (i > 0)
			*y += UI_DESC_TEXT_LINE_SPACING;
		VB2_TRY(ui_get_bitmap(desc->files[i], locale_code, 0, &bitmap));
		VB2_TRY(ui_draw_desc_bitmap(&bitmap, *y, reverse));
		*y += ui_get_desc_height(&bitmap);
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

vb2_error_t ui_draw_textbox(const char *str, int32_t *y, int32_t min_lines,
			    int32_t min_height)
{
	vb2_error_t rv = VB2_SUCCESS;
	int32_t x;
	int32_t y_base = *y;
	int32_t text_height = UI_BOX_TEXT_HEIGHT;
	int32_t line_height = UI_BOX_TEXT_LINE_HEIGHT;
	int num_lines;
	int32_t max_content_height, content_width;
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
		if (*ptr == '\t')
			*ptr = ' ';

	num_lines = MAX(count_lines(buf), min_lines);

	 /* Simply ensure that the textbox won't exceed the screen. */
	max_content_height = UI_SCALE - *y - 2 * UI_BOX_PADDING_V;
	if (line_height * num_lines > max_content_height) {
		line_height = max_content_height / num_lines;
		text_height = UI_BOX_TEXT_HEIGHT * line_height /
			UI_BOX_TEXT_LINE_HEIGHT;
	}

	x = UI_MARGIN_H;
	box_width = UI_SCALE - UI_MARGIN_H * 2;
	content_width = box_width - UI_BOX_PADDING_H * 2;
	box_height = line_height * num_lines + UI_BOX_PADDING_V * 2;
	box_height = MAX(box_height, min_height);

	/* Clear printing area. */
	ui_draw_rounded_box(x, *y, box_width, box_height,
			    &ui_color_bg, 0, 0, 0);
	/* Draw the box. */
	ui_draw_rounded_box(x, *y, box_width, box_height, &ui_color_textbox_bg,
			    0, UI_BOX_RADIUS, 0);

	x += UI_BOX_PADDING_H;
	*y += UI_BOX_PADDING_V;

	end = buf;
	while ((line = strsep(&end, "\n"))) {
		vb2_error_t line_rv;
		int32_t line_width;
		int32_t h = text_height;
		/* Ensure the text width is no more than box width */
		line_rv = ui_get_text_width(line, h, &line_width);
		if (line_rv) {
			/* Save the first error in rv */
			if (rv == VB2_SUCCESS)
				rv = line_rv;
			continue;
		}
		if (line_width > content_width)
			h = h * content_width / line_width;
		line_rv = ui_draw_text(line, x, *y + line_height / 2, h,
				       &ui_color_textbox_bg, &ui_color_fg,
				       PIVOT_H_LEFT | PIVOT_V_CENTER, 0);
		*y += line_height;
		/* Save the first error in rv */
		if (line_rv && rv == VB2_SUCCESS)
			rv = line_rv;
	}

	*y = y_base + box_height;
	free(buf);
	return rv;
}

vb2_error_t ui_draw_auto_wrap_textbox(const char *str, size_t str_len, int32_t *y,
				      int32_t num_lines, uint32_t chars_per_line,
				      int32_t min_height)
{
	vb2_error_t rv = VB2_SUCCESS;
	int32_t x;
	int32_t y_base = *y;
	int32_t text_height = UI_BOX_TEXT_HEIGHT;
	int32_t line_height = UI_BOX_TEXT_LINE_HEIGHT;
	int32_t max_content_height, content_width;
	int32_t box_width, box_height;
	const char *line;
	const char *end = str + str_len;

	/* Simply ensure that the textbox won't exceed the screen. */
	max_content_height = UI_SCALE - *y - 2 * UI_BOX_PADDING_V;
	if (line_height * num_lines > max_content_height) {
		line_height = max_content_height / num_lines;
		text_height = UI_BOX_TEXT_HEIGHT * line_height /
			UI_BOX_TEXT_LINE_HEIGHT;
	}

	x = UI_MARGIN_H;
	box_width = UI_SCALE - UI_MARGIN_H * 2;
	content_width = box_width - UI_BOX_PADDING_H * 2;
	box_height = line_height * num_lines + UI_BOX_PADDING_V * 2;
	box_height = MAX(box_height, min_height);

	/* Clear printing area. */
	ui_draw_rounded_box(x, *y, box_width, box_height,
			    &ui_color_bg, 0, 0, 0);
	/* Draw the box. */
	ui_draw_rounded_box(x, *y, box_width, box_height, &ui_color_textbox_bg,
			    0, UI_BOX_RADIUS, 0);

	x += UI_BOX_PADDING_H;
	*y += UI_BOX_PADDING_V;

	for (line = str; str < end && *str && num_lines; line = str, num_lines--) {
		vb2_error_t line_rv;
		int32_t line_width;
		int32_t h = text_height;
		size_t n;
		for (n = 0; n < chars_per_line; n++, str++) {
			if (str >= end || *str == '\0')
				break;
			if (*str == '\n') {
				str++;
				break;
			}
		}
		if (n == chars_per_line && *str == '\n' && str < end)
			/* We will print this new line now, skip it */
			str++;

		/* Ensure the text width is no more than box width */
		line_rv = ui_get_ntext_width(line, n, h, &line_width);
		if (line_rv) {
			/* Save the first error in rv */
			if (rv == VB2_SUCCESS)
				rv = line_rv;
			continue;
		}
		if (line_width > content_width)
			h = h * content_width / line_width;
		line_rv = ui_draw_ntext(line, n, x, *y + line_height / 2, h,
					&ui_color_textbox_bg, &ui_color_fg,
					PIVOT_H_LEFT | PIVOT_V_CENTER, 0);
		*y += line_height;
		/* Save the first error in rv */
		if (line_rv && rv == VB2_SUCCESS)
			rv = line_rv;
	}

	*y = y_base + box_height;
	return rv;
}

vb2_error_t ui_get_textbox_chars_per_line(uint32_t *chars_per_line)
{
	int32_t char_width;

	VB2_TRY(ui_get_text_width("?", UI_BOX_TEXT_HEIGHT, &char_width));

	*chars_per_line = (UI_SCALE - UI_MARGIN_H * 2 - UI_BOX_PADDING_H * 2) /
			  char_width;

	return VB2_SUCCESS;
}

vb2_error_t ui_get_textbox_lines_per_page(int32_t y, uint32_t *lines_per_page)
{
	/* The textbox extends to the bottom margin of the screen. */
	const int32_t textbox_height = UI_SCALE - UI_BOX_MARGIN_BOTTOM - y;

	*lines_per_page = (textbox_height - UI_BOX_PADDING_V * 2) /
			  UI_BOX_TEXT_LINE_HEIGHT;

	return VB2_SUCCESS;
}

vb2_error_t ui_get_log_textbox_dimensions(enum ui_screen screen,
					  const char *locale_code,
					  uint32_t *lines_per_page,
					  uint32_t *chars_per_line)
{
	const struct ui_screen_info *screen_info;
	struct ui_bitmap bitmap;
	int32_t title_height;
	int32_t above_textbox_height;

	screen_info = ui_get_screen_info(screen);
	VB2_TRY(ui_get_bitmap(screen_info->title, locale_code, 0, &bitmap));

	/* Log screens use the full view layout. */
	title_height = ui_get_text_block_height(&bitmap, UI_TITLE_TEXT_HEIGHT,
						UI_TITLE_TEXT_LINE_HEIGHT);
	above_textbox_height = UI_MARGIN_TOP + UI_TITLE_MARGIN_V * 2 +
		title_height + UI_NAVIGATION_BAR_HEIGHT;

	VB2_TRY(ui_get_textbox_lines_per_page(above_textbox_height,
					      lines_per_page));

	return ui_get_textbox_chars_per_line(chars_per_line);
}

vb2_error_t ui_draw_textbox_with_scrollbar(const char *str, size_t n,
					   const struct ui_state *state, int32_t *y,
					   int32_t first_item, size_t total_items,
					   size_t items_per_page, bool wrap_lines)
{
	uint32_t lines_per_page;
	uint32_t chars_per_line;
	const int32_t y_base = *y;
	const int32_t box_height = UI_SCALE - UI_BOX_MARGIN_BOTTOM - y_base;
	int32_t scrollbar_y, scrollbar_height;
	const int32_t log_box_width = UI_SCALE - UI_MARGIN_H * 2;

	VB2_TRY(ui_get_textbox_lines_per_page(*y, &lines_per_page));
	VB2_TRY(ui_get_textbox_chars_per_line(&chars_per_line));
	if (wrap_lines)
		VB2_TRY(ui_draw_auto_wrap_textbox(str, n, y, lines_per_page,
						  chars_per_line, box_height));
	else
		VB2_TRY(ui_draw_textbox(str, y, lines_per_page, box_height));

	/* Align the scrollbar with the text. */
	scrollbar_y = y_base + UI_BOX_PADDING_V;
	scrollbar_height = box_height - UI_BOX_PADDING_V * 2;

	/* No scrollbar if there is only one page. Clear any previous scrollbar. */
	if (total_items <= items_per_page) {
		const int32_t scrollbar_x = UI_MARGIN_H + log_box_width +
					    UI_SCROLLBAR_MARGIN_LEFT;
		return ui_draw_box(scrollbar_x, scrollbar_y, UI_SCROLLBAR_WIDTH,
				   scrollbar_height, &ui_color_bg, 0);
	}

	return ui_draw_scrollbar(UI_MARGIN_H + log_box_width, scrollbar_y,
				 scrollbar_height, first_item, total_items,
				 items_per_page, &ui_color_textbox_bg, 0);
}

vb2_error_t ui_draw_log_textbox(const char *str, const struct ui_state *state,
				int32_t *y)
{
	return ui_draw_textbox_with_scrollbar(str, 0, state, y, state->current_page,
					      state->log.impl.static_log.page_count, 1, false);
}

vb2_error_t ui_draw_scrollbar(int32_t begin_x, int32_t begin_y, int32_t total_h,
			      int32_t first_item_index, size_t items_count,
			      size_t items_per_page,
			      const struct rgb_color *track_color, int reverse)
{
	int32_t h, y, movable_height;
	if (items_count <= 1)
		return VB2_SUCCESS;

	if (first_item_index + items_per_page > items_count) {
		UI_WARN("Scrollbar first_item_index + items_count > "
			"items_per_page (%d + %zu > %zu), change "
			"first_item_idx to items_count - items_per_page (%zu)\n",
			first_item_index, items_count, items_per_page,
			items_count - items_per_page);
		first_item_index = items_count - items_per_page;
	}

	/*
	 * Make sure the height of the scrollbar won't be too small. If total_h
	 * is less than or equal to UI_SCROLLBAR_MIN_HEIGHT, the movable_height
	 * will be 0, and the following code will draw a full scrollbar which
	 * can't be moved.
	 */
	h = MIN(MAX(total_h * items_per_page / items_count,
		    UI_SCROLLBAR_MIN_HEIGHT),
		total_h);
	movable_height = total_h - h;
	y = begin_y +
	    movable_height * first_item_index / (items_count - items_per_page);

	const int32_t scrollbar_x = begin_x + UI_SCROLLBAR_MARGIN_LEFT;

	/* Draw background track */
	VB2_TRY(ui_draw_rounded_box(scrollbar_x, begin_y, UI_SCROLLBAR_WIDTH,
				    total_h, track_color, 0,
				    UI_SCROLLBAR_CORNER_RADIUS, reverse));
	/* Draw scrollbar thumb */
	VB2_TRY(ui_draw_rounded_box(scrollbar_x, y, UI_SCROLLBAR_WIDTH, h,
				    &ui_color_scrollbar, 0,
				    UI_SCROLLBAR_CORNER_RADIUS, reverse));

	return VB2_SUCCESS;
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
	const int32_t y = UI_MARGIN_TOP + UI_LANG_MARGIN_V +
		UI_H2_BUTTON_HEIGHT +
		(UI_LANG_MARGIN_V + UI_TITLE_MARGIN_V) / 2;

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
			       int32_t y,
			       int32_t *out_focused_item_y)
{
	int i;
	int32_t x;
	const struct ui_menu_state *ms = &state->menu_state;

	*out_focused_item_y = 0;

	/* H1 and H2 dropdown trigger buttons */
	x = UI_MARGIN_H;
	bool prev_is_h1 = false;
	for (i = 0; i < menu->num_items; i++) {
		if (UI_GET_BIT(ms->hidden_item_mask, i))
			continue;

		const struct ui_menu_item *item = &menu->items[i];
		if (item->type != UI_MENU_ITEM_TYPE_H1 &&
		    item->type != UI_MENU_ITEM_TYPE_H2_DROPDOWN)
			continue;

		if (item->type == UI_MENU_ITEM_TYPE_H2_DROPDOWN && prev_is_h1)
			y += UI_DROPDOWN_MARGIN_TOP;

		if (i == ms->focused_item)
			*out_focused_item_y = y;

		bool is_focused = is_menu_item_focused(state, i);

		if (item->type == UI_MENU_ITEM_TYPE_H1) {
			VB2_TRY(ui_draw_h1_button(item, state, x, y,
						  UI_H1_BUTTON_HEIGHT,
						  is_focused,
						  UI_GET_BIT(ms->disabled_item_mask, i)));
		} else {
			bool is_expanded = is_dropdown_expanded(state, i);
			VB2_TRY(ui_draw_h2_dropdown(item, state, x, y,
						    UI_H2_BUTTON_HEIGHT,
						    is_focused, is_expanded));
		}

		prev_is_h1 = (item->type == UI_MENU_ITEM_TYPE_H1);
		y += (prev_is_h1 ?
		      UI_H1_BUTTON_HEIGHT : UI_H2_BUTTON_HEIGHT) +
		     UI_BUTTON_MARGIN_V;
	}

	/* H3 buttons (anchored to bottom) */
	x = UI_MARGIN_H;
	VB2_TRY(get_footer_y(state->locale->code, &y));
	y -= UI_FOOTER_MARGIN_TOP + UI_H3_BUTTON_HEIGHT;
	for (i = menu->num_items - 1; i >= 0; i--) {
		if (UI_GET_BIT(ms->hidden_item_mask, i))
			continue;
		if (menu->items[i].type != UI_MENU_ITEM_TYPE_H3)
			continue;
		if (i == ms->focused_item)
			*out_focused_item_y = y;
		VB2_TRY(ui_draw_h3_button(&menu->items[i], state,
					  x, y, UI_H3_BUTTON_HEIGHT,
					  ms->focused_item == i));
		y -= UI_H3_BUTTON_HEIGHT + UI_BUTTON_MARGIN_V;
	}

	return VB2_SUCCESS;
}

/*
 * Draw a focus border for a sub-menu item.
 *
 * @param x		x-coordinate of the box.
 * @param y		y-coordinate of the box.
 * @param width		Width of the box.
 * @param height	Height of the box.
 * @param is_first	True if this is the first item.
 * @param is_last	True if this is the last item.
 * @param reverse	RTL flag.
 */
static vb2_error_t ui_draw_sub_menu_item_focus(int32_t x, int32_t y,
					       int32_t width, int32_t height,
					       bool is_first, bool is_last,
					       int reverse)
{
	const uint32_t radius = UI_SUB_MENU_BORDER_RADIUS;
	const uint32_t thickness = UI_BUTTON_FOCUS_RING_THICKNESS;
	const struct rgb_color *border_color = &ui_color_button_focus_ring;
	const struct rgb_color *bg_color = &ui_color_sub_menu_bg;
	/* Single/first/last items use radius; middle items have square corners (0). */
	uint32_t r = (is_first || is_last) ? radius : 0;

	VB2_TRY(ui_draw_rounded_box(x, y, width, height, border_color,
				    thickness, r, reverse));

	/* If both ends (single item) or neither (middle item), we are done. */
	if (is_first == is_last)
		return VB2_SUCCESS;

	/* Square off bottom corners (if first item) or top corners (if last item). */
	int32_t corner_y = is_first ? (y + height - radius) : y;
	int32_t h_border_y = is_first ? (y + height - thickness) : y;

	/* Left corner */
	VB2_TRY(ui_draw_box(x, corner_y, radius, radius, bg_color, reverse));
	VB2_TRY(ui_draw_box(x, corner_y, thickness, radius,
			    border_color, reverse));
	VB2_TRY(ui_draw_box(x, h_border_y, radius, thickness,
			    border_color, reverse));

	/* Right corner */
	VB2_TRY(ui_draw_box(x + width - radius, corner_y, radius, radius,
			    bg_color, reverse));
	VB2_TRY(ui_draw_box(x + width - thickness, corner_y, thickness, radius,
			    border_color, reverse));
	VB2_TRY(ui_draw_box(x + width - radius, h_border_y, radius, thickness,
			    border_color, reverse));

	return VB2_SUCCESS;
}

/*
 * Calculate the required width of a sub-menu based on its items.
 *
 * @param menu		Sub-menu.
 * @param menu_state	Sub-menu state (for hidden items).
 * @param state		UI state.
 * @param has_scrollbar	Whether the sub-menu will display a scrollbar.
 * @param out_width	Calculated width.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
static vb2_error_t ui_get_sub_menu_width(const struct ui_menu *menu,
					 const struct ui_menu_state *menu_state,
					 const struct ui_state *state,
					 bool has_scrollbar,
					 int32_t *out_width)
{
	int32_t max_item_width = 0;

	for (size_t i = 0; i < menu->num_items; i++) {
		if (UI_GET_BIT(menu_state->hidden_item_mask, i))
			continue;

		const struct ui_menu_item *item = &menu->items[i];
		const char *file;
		struct ui_bitmap bitmap;
		const char *locale_code = get_item_locale_code(item, state);
		int32_t text_width;

		VB2_TRY(get_item_file(item, state, &file));
		if (!file) {
			UI_ERROR("Sub-menu item #%zu: no .file\n", i);
			return VB2_ERROR_UI_DRAW_FAILURE;
		}

		VB2_TRY(ui_get_bitmap(file, locale_code, 0, &bitmap));
		VB2_TRY(ui_get_bitmap_width(&bitmap,
					    UI_BUTTON_TEXT_HEIGHT,
					    &text_width));
		max_item_width = MAX(max_item_width, text_width);
	}

	int32_t padding = UI_SUB_MENU_PADDING_H * 2;
	int32_t width = max_item_width + padding;
	width = MAX(width, UI_SUB_MENU_MIN_WIDTH);

	int32_t total_width = width;
	if (has_scrollbar)
		total_width += UI_SCROLLBAR_MARGIN_LEFT + UI_SCROLLBAR_WIDTH;

	const int32_t max_width = UI_SCALE - UI_MARGIN_H * 2;
	if (total_width > max_width) {
		UI_ERROR("Sub-menu width %d exceeds max width %d\n",
			 total_width, max_width);
		return VB2_ERROR_UI_DRAW_FAILURE;
	}

	*out_width = width;
	return VB2_SUCCESS;
}

static vb2_error_t ui_draw_sub_menu(struct ui_context *ui,
				    const struct ui_menu_state *menu_state,
				    int32_t focused_item_y)
{
	if (!menu_state || !menu_state->menu || menu_state->menu->num_items == 0) {
		UI_WARN("Sub-menu is empty or uninitialized\n");
		return VB2_SUCCESS;
	}

	const struct ui_menu *sub_menu = menu_state->menu;
	const struct ui_state *state = ui->state;
	const int reverse = state->locale->rtl;
	size_t total_items = 0;
	size_t focused_pos = 0;

	for (size_t i = 0; i < sub_menu->num_items; i++) {
		if (UI_GET_BIT(menu_state->hidden_item_mask, i))
			continue;
		if (i == menu_state->focused_item)
			focused_pos = total_items;
		total_items++;
	}
	if (total_items == 0)
		return VB2_SUCCESS;

	int32_t box_x = UI_MARGIN_H;
	int32_t item_h = UI_SUB_MENU_ITEM_HEIGHT;
	int32_t box_y = focused_item_y + UI_H2_BUTTON_HEIGHT +
		UI_SUB_MENU_PADDING_V;

	int32_t max_available_h = UI_SCALE - box_y;
	if (max_available_h < item_h) {
		UI_ERROR("No vertical space available for sub-menu (%d < %d)\n",
			 max_available_h, item_h);
		return VB2_ERROR_UI_DRAW_FAILURE;
	}

	size_t max_items = MIN((size_t)(max_available_h / item_h),
			       UI_SUB_MENU_MAX_ITEMS);
	size_t items_per_page = MIN(total_items, max_items);
	int32_t box_h = item_h * items_per_page;
	bool has_scrollbar = total_items > items_per_page;

	int32_t box_w;
	VB2_TRY(ui_get_sub_menu_width(sub_menu, menu_state, state,
				      has_scrollbar, &box_w));

	/*
	 * Calculate the visible window [id_begin, id_begin + items_per_page)
	 * of items to display. The focused item is kept in view and placed
	 * at the center if possible.
	 */
	size_t id_begin = 0;
	if (total_items > items_per_page) {
		size_t target_pos = (items_per_page - 1) / 2;
		if (focused_pos > target_pos)
			id_begin = MIN(focused_pos - target_pos,
				       total_items - items_per_page);
	}

	/* Background container card */
	VB2_TRY(ui_draw_rounded_box(box_x, box_y, box_w, box_h,
				    &ui_color_sub_menu_bg, 0,
				    UI_SUB_MENU_BORDER_RADIUS, reverse));

	size_t pos = 0, slot = 0;
	for (size_t i = 0; i < sub_menu->num_items; i++) {
		if (UI_GET_BIT(menu_state->hidden_item_mask, i))
			continue;

		if (pos++ < id_begin)
			continue;
		if (slot >= items_per_page)
			break;

		int32_t item_y = box_y + item_h * slot;
		const struct ui_menu_item *item = &sub_menu->items[i];
		bool is_focused = !menu_state->trigger_focused &&
				  (i == menu_state->focused_item);

		if (is_focused) {
			VB2_TRY(ui_draw_sub_menu_item_focus(
				box_x, item_y, box_w, item_h,
				slot == 0,
				slot == items_per_page - 1,
				reverse));
		}

		/* Draw item text */
		int32_t text_x = box_x + UI_SUB_MENU_PADDING_H;
		int32_t text_y = item_y + item_h / 2;
		const char *file;
		struct ui_bitmap bitmap;
		const char *locale_code = get_item_locale_code(item, state);

		VB2_TRY(get_item_file(item, state, &file));
		if (!file) {
			UI_ERROR("Sub-menu item #%zu: no .file\n", i);
			return VB2_ERROR_UI_DRAW_FAILURE;
		}
		VB2_TRY(ui_get_bitmap(file, locale_code, 0, &bitmap));
		VB2_TRY(ui_draw_mapped_bitmap(&bitmap, text_x, text_y,
					      UI_SIZE_AUTO,
					      UI_BUTTON_TEXT_HEIGHT,
					      &ui_color_sub_menu_bg,
					      &ui_color_fg,
					      PIVOT_H_LEFT | PIVOT_V_CENTER,
					      reverse));

		slot++;
	}

	/* Draw scrollbar if there are more items than fit on the page */
	if (total_items > items_per_page) {
		VB2_TRY(ui_draw_scrollbar(box_x + box_w, box_y,
					  box_h, id_begin,
					  total_items, items_per_page,
					  &ui_color_scrollbar_track, reverse));
	}

	return VB2_SUCCESS;
}

vb2_error_t ui_draw_default(struct ui_context *ui,
			    const struct ui_state *prev_state)
{
	const struct ui_state *state = ui->state;
	const struct ui_screen_info *screen = state->screen;
	const struct ui_menu *menu = ui_get_menu(ui);
	const struct ui_menu_state *ms = &state->menu_state;
	const bool closing_sub_menu = prev_state &&
				      prev_state->is_sub_menu_active &&
				      !state->is_sub_menu_active;
	const bool has_lang_dropdown =
		menu->num_items > 0 &&
		menu->items[0].type == UI_MENU_ITEM_TYPE_LANGUAGE;

	const char *locale_code = state->locale->code;
	const int reverse = state->locale->rtl;
	int focused;
	int32_t x, y;
	const int32_t w = UI_SIZE_AUTO;
	int32_t h;
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
		if (prev_state->screen->is_fullview || screen->is_fullview) {
			/*
			 * The screens with is_fullview property use different
			 * layout.
			 */
			clear_screen(&ui_color_bg);
		} else {
			/* Clear everything above the footer for new screen. */
			int32_t footer_y;

			VB2_TRY(get_footer_y(locale_code, &footer_y));
			VB2_TRY(ui_draw_box(0, 0, UI_SCALE, footer_y,
					    &ui_color_bg, 0));
		}
	} else if (closing_sub_menu) {
		/*
		 * Clear everything below the language dropdown header. The
		 * sub-menu only expands downwards and may overlap
		 * with H3 buttons and the footer, while the language
		 * dropdown at the top remains untouched and does not need to
		 * be redrawn.
		 */
		const int32_t clear_y = UI_MARGIN_TOP + UI_LANG_MARGIN_V +
			UI_H2_BUTTON_HEIGHT;
		VB2_TRY(ui_draw_box(0, clear_y, UI_SCALE, UI_SCALE - clear_y,
				    &ui_color_bg, 0));
	}

	/* Warning if we are in recovery and using dev signed keys. */
	if (!screen->is_fullview)
		VB2_TRY(ui_draw_dev_signed_warning());

	/* Language dropdown header */
	if (has_lang_dropdown) {
		focused = is_menu_item_focused(state, 0);
		bool prev_focused = is_menu_item_focused(prev_state, 0);
		bool expanded = is_dropdown_expanded(state, 0);
		bool prev_expanded = is_dropdown_expanded(prev_state, 0);

		if (!prev_state ||
		    prev_state->screen != state->screen ||
		    prev_state->locale != state->locale ||
		    prev_state->error_code != state->error_code ||
		    prev_focused != focused ||
		    prev_expanded != expanded) {
			VB2_TRY(ui_draw_h2_dropdown(
				&menu->items[0], state, UI_MARGIN_H,
				UI_MARGIN_TOP + UI_LANG_MARGIN_V,
				UI_H2_BUTTON_HEIGHT, focused, expanded));
		}
	}

	/*
	 * Draw the footer if previous screen doesn't have a footer, or if
	 * locale changed, or if a sub-menu was closed.
	 */
	if (!screen->no_footer &&
	    (!prev_state ||
	     prev_state->screen->no_footer ||
	     prev_state->locale != state->locale ||
	     prev_state->error_code != state->error_code ||
	     closing_sub_menu))
		VB2_TRY(draw_footer(state));

	x = UI_MARGIN_H;
	y = UI_MARGIN_TOP;
	if (has_lang_dropdown)
		y += UI_LANG_MARGIN_V * 2 + UI_H2_BUTTON_HEIGHT;

	/* Screen icon */
	if (screen->icon != UI_ICON_TYPE_NONE) {
		switch (screen->icon) {
		case UI_ICON_TYPE_CONSTRUCTION:
			icon_file = "ic_construction.bmp";
			break;
		default:
			UI_ERROR("Unknown icon type: %d\n", screen->icon);
			return VB2_ERROR_UI_DRAW_FAILURE;
		}
		VB2_TRY(ui_get_bitmap(icon_file, NULL, 0, &bitmap));
		VB2_TRY(ui_draw_mapped_bitmap(
			&bitmap,
			x + UI_ICON_BOX_SIZE / 2,
			y + UI_ICON_MARGIN_V + UI_ICON_BOX_SIZE / 2,
			UI_ICON_HEIGHT, UI_ICON_HEIGHT,
			&ui_color_bg, &ui_color_icon,
			PIVOT_H_CENTER | PIVOT_V_CENTER, reverse));
		y += UI_ICON_MARGIN_V * 2 + UI_ICON_BOX_SIZE;
	}

	/* Title */
	if (screen->title) {
		y += UI_TITLE_MARGIN_V;
		VB2_TRY(ui_get_bitmap(screen->title, locale_code, 0, &bitmap));
		h = ui_get_text_block_height(&bitmap, UI_TITLE_TEXT_HEIGHT,
					     UI_TITLE_TEXT_LINE_HEIGHT);
		VB2_TRY(ui_draw_bitmap(&bitmap, x, y + h / 2, w,
				       UI_TITLE_TEXT_HEIGHT *
				       ui_get_bitmap_num_lines(&bitmap),
				       PIVOT_H_LEFT | PIVOT_V_CENTER, reverse));
		y += h + UI_TITLE_MARGIN_V;
	}

	/* Navigation bar */
	if (screen->is_fullview) {
		VB2_TRY(draw_navigation_bar(state, y));
		y += UI_NAVIGATION_BAR_HEIGHT;
	}

	/* Description */
	if (screen->draw_desc)
		VB2_TRY(screen->draw_desc(ui, prev_state, &y));
	else
		VB2_TRY(ui_draw_desc(&screen->desc, state, &y));
	y += UI_DESC_MARGIN_BOTTOM;

	/* Menu buttons */
	if (screen->draw_menu_items) {
		VB2_TRY(screen->draw_menu_items(ui, prev_state));
	} else {
		int32_t focused_item_y = 0;
		VB2_TRY(ui_draw_menu_items(menu, state, prev_state, y,
					   &focused_item_y));

		if (has_lang_dropdown && ms->focused_item == 0)
			focused_item_y = UI_MARGIN_TOP + UI_LANG_MARGIN_V;

		/* Sub-menu */
		if (state->is_sub_menu_active)
			VB2_TRY(ui_draw_sub_menu(ui, &state->sub_menu_state,
						 focused_item_y));
	}

	return VB2_SUCCESS;
}

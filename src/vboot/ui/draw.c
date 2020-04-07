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
#include <vb2_api.h>

#include "vboot/ui.h"
#include "vboot/util/misc.h"

static vb2_error_t draw(const struct ui_bitmap *bitmap,
			int32_t x, int32_t y, int32_t width, int32_t height,
			uint32_t flags)
{
	struct scale pos = {
		.x = { .n = x, .d = UI_SCALE, },
		.y = { .n = y, .d = UI_SCALE, },
	};
	struct scale dim = {
		.x = { .n = width, .d = UI_SCALE, },
		.y = { .n = height, .d = UI_SCALE, },
	};

	if (get_bitmap_dimension(bitmap->data, bitmap->size, &dim))
		return VB2_ERROR_UI_DRAW_FAILURE;

	if (draw_bitmap(bitmap->data, bitmap->size, &pos, &dim, flags))
		return VB2_ERROR_UI_DRAW_FAILURE;

	return VB2_SUCCESS;
}

static vb2_error_t get_image_size(const struct ui_bitmap *bitmap,
				  int32_t *width, int32_t *height)
{
	struct scale dim = {
		.x = { .n = *width, .d = UI_SCALE, },
		.y = { .n = *height, .d = UI_SCALE, },
	};

	if (get_bitmap_dimension(bitmap->data, bitmap->size, &dim))
		return VB2_ERROR_UI_DRAW_FAILURE;

	/*
	 * Division with round-up to prevent character overlapping problem.
	 * See b/145376714 for more details.
	 */
	*width = DIV_ROUND_UP(dim.x.n * UI_SCALE, dim.x.d);
	*height = DIV_ROUND_UP(dim.y.n * UI_SCALE, dim.y.d);

	return VB2_SUCCESS;
}

static vb2_error_t get_char_width(const char c, int32_t height,
				  enum ui_char_style style,
				  int32_t *width)
{
	struct ui_bitmap bitmap;
	VB2_TRY(ui_get_char_bitmap(c, style, &bitmap));
	*width = UI_SIZE_AUTO;
	VB2_TRY(get_image_size(&bitmap, width, &height));
	return VB2_SUCCESS;
}

static vb2_error_t get_text_width(const char *text, int32_t height,
				  enum ui_char_style style, int32_t *width)
{
	*width = 0;
	while (*text) {
		int32_t char_width;
		vb2_error_t rv = get_char_width(*text, height, style,
						&char_width);
		if (rv) {
			UI_WARN("Failed to retrieve character bitmap of %#.2x, "
				"use '?' to get character width instead\n",
				*text);
			VB2_TRY(get_char_width('?', height, style,
					       &char_width));
		}
		*width += char_width;
		text++;
	}
	return VB2_SUCCESS;
}

/* Draw a line of text. */
static vb2_error_t draw_text(const char *text, int32_t x, int32_t y,
			     int32_t height, uint32_t flags,
			     enum ui_char_style style)
{
	int32_t char_width, char_height;
	struct ui_bitmap bitmap;

	if (flags & PIVOT_H_CENTER || flags & PIVOT_H_RIGHT) {
		char_width = UI_SIZE_AUTO;
		VB2_TRY(get_text_width(text, height, style, &char_width));

		if (flags & PIVOT_H_CENTER) {
			flags &= ~(PIVOT_H_CENTER);
			flags |= PIVOT_H_LEFT;
			x -= char_width / 2;
		} else {
			flags &= ~(PIVOT_H_RIGHT);
			flags |= PIVOT_H_LEFT;
			x -= char_width;
		}
	}

	while (*text) {
		VB2_TRY(ui_get_char_bitmap(*text, style, &bitmap));
		char_width = UI_SIZE_AUTO;
		char_height = height;
		VB2_TRY(get_image_size(&bitmap, &char_width, &char_height));
		VB2_TRY(draw(&bitmap, x, y, UI_SIZE_AUTO, height, flags));
		x += char_width;
		text++;
	}

	return VB2_SUCCESS;
}

static vb2_error_t ui_draw_rounded_box(int32_t x, int32_t y,
				       int32_t w, int32_t h,
				       const struct rgb_color *rgb,
				       uint32_t thickness, uint32_t radius)
{
	struct scale pos_rel = {
		.x = { .n = x, .d = UI_SCALE },
		.y = { .n = y, .d = UI_SCALE },
	};
	struct scale dim_rel = {
		.x = { .n = w, .d = UI_SCALE },
		.y = { .n = h, .d = UI_SCALE },
	};
	struct fraction thickness_rel = {
		.n = thickness,
		.d = UI_SCALE,
	};
	struct fraction radius_rel = {
		.n = radius,
		.d = UI_SCALE,
	};

	/* Convert CBGFX errors to vboot error. */
	if (draw_rounded_box(&pos_rel, &dim_rel, rgb,
			     &thickness_rel, &radius_rel))
		return VB2_ERROR_UI_DRAW_FAILURE;

	return VB2_SUCCESS;
}

static int count_lines(const char *str)
{
	const char *c = str;
	int lines;
	if (!str)
		return 0;

	lines = 1;
	while (*c) {
		if (*c == '\n')
			lines++;
		c++;
	}
	return lines;
}

vb2_error_t ui_print_fallback_message(const char *str)
{
	vb2_error_t rv = VB2_SUCCESS;
	int32_t x, y;
	int32_t max_height = UI_BOX_TEXT_HEIGHT;
	int num_lines;
	int32_t max_total_text_height, box_width, box_height;
	char *buf, *end, *line;
	const enum ui_char_style style = UI_CHAR_STYLE_DEFAULT;

	/* Copy str to buf since strsep() will modify the string. */
	buf = strdup(str);
	if (!buf) {
		UI_ERROR("Failed to malloc string buffer\n");
		return VB2_ERROR_UI_DRAW_FAILURE;
	}

	num_lines = count_lines(buf);
	max_total_text_height = UI_SCALE - UI_BOX_V_MARGIN * 2 -
		UI_BOX_V_PADDING * 2;

	if (max_height * num_lines > max_total_text_height)
		max_height = max_total_text_height / num_lines;

	x = 0;
	y = UI_BOX_V_MARGIN;
	box_width = UI_SCALE;
	box_height = max_height * num_lines + UI_BOX_V_PADDING * 2;

	/* Clear printing area. */
	ui_draw_rounded_box(x, y, box_width, box_height, &ui_color_bg, 0, 0);
	/* Draw the border of a box. */
	ui_draw_rounded_box(x, y, box_width, box_height, &ui_color_fg,
			    UI_BOX_BORDER_THICKNESS, UI_BOX_BORDER_RADIUS);

	x += UI_BOX_H_PADDING;
	y += UI_BOX_V_PADDING;

	end = buf;
	while ((line = strsep(&end, "\n"))) {
		vb2_error_t line_rv;
		int32_t width;
		int32_t height = max_height;
		/* Ensure the text width is no more than box width */
		line_rv = get_text_width(line, height, style, &width);
		if (line_rv) {
			/* Save the first error in rv */
			if (rv == VB2_SUCCESS)
				rv = line_rv;
			continue;
		}
		if (width > box_width)
			height = height * box_width / width;
		line_rv = draw_text(line, x, y, height,
				    PIVOT_H_LEFT | PIVOT_V_TOP, style);
		y += height;
		/* Save the first error in rv */
		if (line_rv && rv == VB2_SUCCESS)
			rv = line_rv;
	}

	free(buf);
	return rv;
}

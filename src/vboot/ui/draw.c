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

vb2_error_t ui_get_text_width(const char *text, int32_t height,
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

vb2_error_t ui_draw_text(const char *text, int32_t x, int32_t y,
			 int32_t height, uint32_t flags,
			 enum ui_char_style style, int reverse)
{
	int32_t char_width, char_height;
	struct ui_bitmap bitmap;

	if (flags & PIVOT_H_CENTER || flags & PIVOT_H_RIGHT) {
		char_width = UI_SIZE_AUTO;
		VB2_TRY(ui_get_text_width(text, height, style, &char_width));

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

vb2_error_t ui_draw_rounded_box(int32_t x, int32_t y,
				int32_t width, int32_t height,
				const struct rgb_color *rgb,
				uint32_t thickness, uint32_t radius)
{
	struct scale pos_rel = {
		.x = { .n = x, .d = UI_SCALE },
		.y = { .n = y, .d = UI_SCALE },
	};
	struct scale dim_rel = {
		.x = { .n = width, .d = UI_SCALE },
		.y = { .n = height, .d = UI_SCALE },
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

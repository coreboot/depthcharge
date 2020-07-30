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

#include <ctype.h>
#include <libpayload.h>
#include <vb2_api.h>

#include "vboot/ui.h"

#define SCREEN_FRACTION(numerator) ((struct fraction){	\
	.n = numerator,					\
	.d = UI_SCALE,					\
})

#define SCALE(x_rel, y_rel) ((struct scale){	\
	.x = SCREEN_FRACTION(x_rel),		\
	.y = SCREEN_FRACTION(y_rel),		\
})

#define BMP_HEADER_OFFSET_NUM_LINES 6

static uint32_t reverse_pivot(uint32_t pivot) {
	uint32_t left = pivot & PIVOT_H_LEFT;

	/* Swap PIVOT_H_LEFT and PIVOT_H_RIGHT */
	if (pivot & PIVOT_H_RIGHT)
		pivot |= PIVOT_H_LEFT;
	else
		pivot &= ~PIVOT_H_LEFT;

	if (left)
		pivot |= PIVOT_H_RIGHT;
	else
		pivot &= ~PIVOT_H_RIGHT;

	return pivot;
}

vb2_error_t ui_draw_bitmap(const struct ui_bitmap *bitmap,
			   int32_t x, int32_t y, int32_t width, int32_t height,
			   uint32_t flags, int reverse)
{
	int ret;

	if (reverse) {
		x = UI_SCALE - x;
		flags = reverse_pivot(flags);
	}

	struct scale pos = SCALE(x, y);
	struct scale dim = SCALE(width, height);

	ret = draw_bitmap(bitmap->data, bitmap->size, &pos, &dim, flags);

	if (ret == CBGFX_ERROR_BOUNDARY) {
		/*
		 * Fallback mechanism: try again with smaller width.
		 * WARNING: The following message might be grepped in FAFT.
		 * DO NOT MODIFY it.
		 */
		UI_WARN("Drawing bitmap '%s' (x=%d, y=%d, w=%d, h=%d, f=%#x) "
			"exceeded canvas; try scaling\n", bitmap->name,
			 x, y, width, height, flags);
		if (flags & PIVOT_H_LEFT)
			width = UI_SCALE - UI_MARGIN_H - x;
		else if (flags & PIVOT_H_RIGHT)
			width = x - UI_MARGIN_H;
		else
			width = MIN(UI_SCALE - UI_MARGIN_H - x,
				    x - UI_MARGIN_H) * 2;
		dim.x.n = width;
		ret = draw_bitmap(bitmap->data, bitmap->size, &pos, &dim,
				  flags);
	}

	if (ret) {
		UI_ERROR("Drawing bitmap '%s' (x=%d, y=%d, w=%d, h=%d, f=%#x) "
			 "failed: %#x\n", bitmap->name,
			 x, y, width, height, flags, ret);
		return VB2_ERROR_UI_DRAW_FAILURE;
	}

	return VB2_SUCCESS;
}

vb2_error_t ui_draw_mapped_bitmap(const struct ui_bitmap *bitmap,
				  int32_t x, int32_t y,
				  int32_t width, int32_t height,
				  const struct rgb_color *bg_color,
				  const struct rgb_color *fg_color,
				  uint32_t flags, int reverse)
{
	vb2_error_t rv;
	if (set_color_map(bg_color, fg_color))
		return VB2_ERROR_UI_DRAW_FAILURE;
	rv = ui_draw_bitmap(bitmap, x, y, width, height, flags, reverse);
	clear_color_map();
	return rv;
}

/*
 * Get bitmap size.
 *
 * If the input width is UI_SIZE_AUTO, it will be derived from the input height
 * to keep the aspect ratio, and vice versa. If both the input width and height
 * are UI_SIZE_AUTO, the original bitmap size will be returned.
 *
 * @param bitmap	Pointer to the bitmap struct.
 * @param width		Width of the image to be calculated.
 * @param height	Height of the image to be calculated.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
static vb2_error_t ui_get_bitmap_size(const struct ui_bitmap *bitmap,
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

vb2_error_t ui_get_bitmap_width(const struct ui_bitmap *bitmap,
				int32_t height, int32_t *width)
{
	*width = UI_SIZE_AUTO;
	VB2_TRY(ui_get_bitmap_size(bitmap, width, &height));
	return VB2_SUCCESS;
}

/*
 * The BMP header leaves bytes at offsets 6-9 as "Reserved; actual
 * value depends on the application that creates the image". So
 * as the bmpblk scripts create these images, they will store the number of
 * text lines at offset 6.
 * See b/158634754 for discussion of alternatives. 
 * https://en.wikipedia.org/wiki/BMP_file_format#Bitmap_file_header
 */
uint32_t ui_get_bitmap_num_lines(const struct ui_bitmap *bitmap)
{
	/* We use first reserved byte of bitmap_file_header. */
	uint8_t num_lines = ((const uint8_t *)bitmap->data)
			    [BMP_HEADER_OFFSET_NUM_LINES];
	if (!num_lines)
		return 1;
	return num_lines;
}

static vb2_error_t get_char_width(const char c, int32_t height, int32_t *width)
{
	struct ui_bitmap bitmap;
	VB2_TRY(ui_get_char_bitmap(c, &bitmap));
	VB2_TRY(ui_get_bitmap_width(&bitmap, height, width));
	return VB2_SUCCESS;
}

vb2_error_t ui_get_text_width(const char *text, int32_t height, int32_t *width)
{
	*width = 0;
	while (*text) {
		int32_t char_width;
		vb2_error_t rv = get_char_width(*text, height, &char_width);
		if (rv) {
			UI_WARN("Failed to retrieve character bitmap of %#.2x, "
				"use '?' to get character width instead\n",
				*text);
			VB2_TRY(get_char_width('?', height, &char_width));
		}
		*width += char_width;
		text++;
	}
	return VB2_SUCCESS;
}

vb2_error_t ui_draw_text(const char *text,
			 int32_t x, int32_t y, int32_t height,
			 const struct rgb_color *bg_color,
			 const struct rgb_color *fg_color,
			 uint32_t flags, int reverse)
{
	int32_t char_width;
	struct ui_bitmap bitmap;

	if (reverse) {
		x = UI_SCALE - x;
		flags = reverse_pivot(flags);
	}

	if (flags & PIVOT_H_CENTER || flags & PIVOT_H_RIGHT) {
		char_width = UI_SIZE_AUTO;
		VB2_TRY(ui_get_text_width(text, height, &char_width));

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
		/* Replace a non-printable character with a "?". */
		VB2_TRY(ui_get_char_bitmap(isprint(*text) ? *text : '?',
					   &bitmap));
		VB2_TRY(ui_get_bitmap_width(&bitmap, height, &char_width));
		VB2_TRY(ui_draw_mapped_bitmap(&bitmap, x, y,
					      UI_SIZE_AUTO, height,
					      bg_color, fg_color, flags, 0));
		x += char_width;
		text++;
	}

	return VB2_SUCCESS;
}

vb2_error_t ui_draw_rounded_box(int32_t x, int32_t y,
				int32_t width, int32_t height,
				const struct rgb_color *rgb,
				uint32_t thickness, uint32_t radius,
				int reverse)
{
	if (reverse)
		x = UI_SCALE - x - width;

	struct scale pos_rel = SCALE(x, y);
	struct scale dim_rel = SCALE(width, height);
	struct fraction thickness_rel = SCREEN_FRACTION(thickness);
	struct fraction radius_rel = SCREEN_FRACTION(radius);

	/* Convert CBGFX errors to vboot error. */
	if (draw_rounded_box(&pos_rel, &dim_rel, rgb,
			     &thickness_rel, &radius_rel))
		return VB2_ERROR_UI_DRAW_FAILURE;

	return VB2_SUCCESS;
}

vb2_error_t ui_draw_box(int32_t x, int32_t y,
			int32_t width, int32_t height,
			const struct rgb_color *rgb,
			int reverse)
{
	return ui_draw_rounded_box(x, y, width, height, rgb, 0, 0, reverse);
}

vb2_error_t ui_draw_h_line(int32_t x, int32_t y,
			   int32_t length, int32_t thickness,
			   const struct rgb_color *rgb)
{
	struct scale pos1 = SCALE(x, y);
	struct scale pos2 = SCALE(x + length, y);
	struct fraction thickness_rel = SCREEN_FRACTION(thickness);

	/* Convert CBGFX errors to vboot error. */
	if (draw_line(&pos1, &pos2, &thickness_rel, rgb))
		return VB2_ERROR_UI_DRAW_FAILURE;

	return VB2_SUCCESS;
}

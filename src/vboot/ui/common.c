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

#include "drivers/video/display.h"
#include "vboot/ui.h"

static vb2_error_t init_screen(void)
{
	static int initialized = 0;
	if (initialized)
		return VB2_SUCCESS;

	/* Make sure framebuffer is initialized before turning display on. */
	clear_screen(&ui_color_bg);
	if (display_init())
		return VB2_ERROR_UI_DISPLAY_INIT;

	initialized = 1;
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

/*
 * Print a fallback message on the top of the screen.
 *
 * A box around the message will also be drawn. The printed text is
 * guaranteed to fit on the screen by adjusting the height of the box,
 * and by resizing individual lines horizontally to fit.
 *
 * @param str		Message to be printed, which may contain newlines.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
static vb2_error_t print_fallback_message(const char *str)
{
	vb2_error_t rv = VB2_SUCCESS;
	int32_t x, y;
	int32_t max_height = UI_BOX_TEXT_HEIGHT;
	int num_lines;
	int32_t max_content_height, content_width, line_spacing = 0;
	int32_t box_width, box_height;
	char *buf, *end, *line;
	const enum ui_char_style style = UI_CHAR_STYLE_DEFAULT;

	/* Copy str to buf since strsep() will modify the string. */
	buf = strdup(str);
	if (!buf) {
		UI_ERROR("Failed to malloc string buffer\n");
		return VB2_ERROR_UI_DRAW_FAILURE;
	}

	num_lines = count_lines(buf);
	if (num_lines < 1)
		num_lines = 1;
	max_content_height = UI_SCALE - UI_BOX_MARGIN_V * 2 -
		UI_BOX_PADDING_V * 2;
	line_spacing = UI_BOX_TEXT_LINE_SPACING * (num_lines - 1);

	if (max_height * num_lines + line_spacing > max_content_height)
		max_height = (max_content_height - line_spacing) / num_lines;

	x = UI_MARGIN_H;
	y = UI_BOX_MARGIN_V;
	box_width = UI_SCALE - UI_MARGIN_H * 2;
	content_width = box_width - UI_BOX_PADDING_H * 2;
	box_height = max_height * num_lines + line_spacing +
		UI_BOX_PADDING_V * 2;

	/* Clear printing area. */
	ui_draw_rounded_box(x, y, box_width, box_height, &ui_color_bg, 0, 0, 0);
	/* Draw the border of a box. */
	ui_draw_rounded_box(x, y, box_width, box_height, &ui_color_fg,
			    UI_BOX_BORDER_THICKNESS, UI_BOX_BORDER_RADIUS, 0);

	x += UI_BOX_PADDING_H;
	y += UI_BOX_PADDING_V;

	end = buf;
	while ((line = strsep(&end, "\n"))) {
		vb2_error_t line_rv;
		int32_t width;
		int32_t height = max_height;
		/* Ensure the text width is no more than box width */
		line_rv = ui_get_text_width(line, height, style, &width);
		if (line_rv) {
			/* Save the first error in rv */
			if (rv == VB2_SUCCESS)
				rv = line_rv;
			continue;
		}
		if (width > content_width)
			height = height * content_width / width;
		line_rv = ui_draw_text(line, x, y, height,
				       PIVOT_H_LEFT | PIVOT_V_TOP, style, 0);
		y += height + UI_BOX_TEXT_LINE_SPACING;
		/* Save the first error in rv */
		if (line_rv && rv == VB2_SUCCESS)
			rv = line_rv;
	}

	free(buf);
	return rv;
}

vb2_error_t ui_display_screen(struct ui_state *state,
			      const struct ui_state *prev_state)
{
	vb2_error_t rv;
	const struct ui_screen_info *screen = state->screen;

	VB2_TRY(init_screen());

	/* If the screen is blank, turn off the backlight; else turn it on. */
	backlight_update(screen->id != VB2_SCREEN_BLANK);

	if (screen->draw)
		rv = screen->draw(state, prev_state);
	else
		rv = ui_draw_default(state, prev_state);

	if (rv) {
		UI_ERROR("Drawing screen %#x failed: %#x", screen->id, rv);
		/* Print fallback message if drawing failed. */
		if (screen->mesg)
			print_fallback_message(screen->mesg);
	}

	return rv;
}

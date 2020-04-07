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

static vb2_error_t print_fallback_message(const char *str)
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
		line_rv = ui_get_text_width(line, height, style, &width);
		if (line_rv) {
			/* Save the first error in rv */
			if (rv == VB2_SUCCESS)
				rv = line_rv;
			continue;
		}
		if (width > box_width)
			height = height * box_width / width;
		line_rv = ui_draw_text(line, x, y, height,
				       PIVOT_H_LEFT | PIVOT_V_TOP, style, 0);
		y += height;
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
	const enum vb2_screen screen = state->screen;
	const struct ui_descriptor *desc;

	if (init_screen())
		return VB2_ERROR_UI_DISPLAY_INIT;

	/* If the screen is blank, turn off the backlight; else turn it on. */
	backlight_update(screen != VB2_SCREEN_BLANK);

	desc = ui_get_descriptor(screen);
	if (!desc) {
		UI_ERROR("Not a valid screen %#x\n", screen);
		return VB2_ERROR_UI_INVALID_SCREEN;
	}

	/* If no drawing function is registered, fallback message will be
	   printed. */
	if (desc->draw) {
		rv = desc->draw(state);
		if (rv)
			UI_ERROR("Drawing failed (%#x)\n", rv);
	} else {
		UI_ERROR("No drawing function registered for screen %#x\n",
			 screen);
		rv = VB2_ERROR_UI_INVALID_SCREEN;
	}

	/* Print fallback message */
	if (rv && desc->mesg)
		print_fallback_message(desc->mesg);

	return rv;
}

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

static const struct ui_error_item error_map[] = {
	[VB2_UI_ERROR_NONE] = {
		.body = NULL,
	},
	[VB2_UI_ERROR_DEV_MODE_ALREADY_ENABLED] = {
		.body = "Dev mode already enabled.",
	},
	[VB2_UI_ERROR_DEBUG_LOG] = {
		.body = "Failed to retrieve debug info.",
	},
	[VB2_UI_ERROR_UNTRUSTED_CONFIRMATION] = {
		.body = "Please use internal keyboard to\n"
			"confirm."
	},
};

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

static vb2_error_t print_error_message(const char *str, const char *locale_code)
{
	vb2_error_t rv = VB2_SUCCESS;
	int32_t x, y;
	int32_t content_width;
	int32_t box_width;
	int32_t button_width;
	char *buf, *end, *line;
	const enum ui_char_style style = UI_CHAR_STYLE_DEFAULT;

	/* Copy str to buf since strsep() will modify the string. */
	buf = strdup(str);
	if (!buf) {
		UI_ERROR("Failed to malloc string buffer\n");
		return VB2_ERROR_UI_MEMORY_ALLOC;
	}

	/* Center the box on the screen */
	x = (UI_SCALE - UI_ERROR_BOX_WIDTH) / 2;
	y = (UI_SCALE - UI_ERROR_BOX_HEIGHT) / 2;
	box_width = UI_SCALE - UI_MARGIN_H * 2;
	content_width = box_width - UI_BOX_PADDING_H * 2;

	/* Clear printing area. */
	ui_draw_rounded_box(x, y,
			    UI_ERROR_BOX_WIDTH,
			    UI_ERROR_BOX_HEIGHT,
			    &ui_color_error_box,
			    0,
			    UI_ERROR_BOX_RADIUS,
			    0);

	x += UI_ERROR_BOX_PADDING_H;
	y += UI_ERROR_BOX_PADDING_V;

	/* Insert icon. */
	struct ui_bitmap bitmap;
	VB2_TRY(ui_get_bitmap("ic_info.bmp", NULL, 0, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y,
			       UI_ERROR_BOX_ICON_HEIGHT,
			       UI_ERROR_BOX_ICON_HEIGHT,
			       PIVOT_H_LEFT | PIVOT_V_TOP,
			       0));

	/* Insert in the body */
	y += UI_ERROR_BOX_SECTION_SPACING + UI_ERROR_BOX_ICON_HEIGHT;
	end = buf;
	while ((line = strsep(&end, "\n"))) {
		vb2_error_t line_rv;
		int32_t width;
		int32_t height = UI_BOX_TEXT_HEIGHT;
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

	/* Insert "Back" button */
	VB2_TRY(ui_get_bitmap("btn_back.bmp", locale_code, 1, &bitmap));
	int32_t text_width;
	VB2_TRY(ui_get_bitmap_width(&bitmap, UI_BUTTON_TEXT_HEIGHT,
				    &text_width));

	button_width = text_width + (UI_BUTTON_TEXT_PADDING_H * 2);
	/* x and y are top-left corner of the button */
	x = (UI_SCALE + UI_ERROR_BOX_WIDTH) / 2 -
		UI_ERROR_BOX_PADDING_H - button_width;
	y = (UI_SCALE + UI_ERROR_BOX_HEIGHT) / 2 -
		UI_ERROR_BOX_PADDING_V - UI_BUTTON_HEIGHT;
	ui_draw_button("btn_back.bmp", locale_code,
		       x, y,
		       button_width,
		       UI_BUTTON_HEIGHT,
		       0, 1);

	return rv;
}

vb2_error_t ui_display_screen(struct ui_state *state,
			      const struct ui_state *prev_state)
{
	vb2_error_t rv;
	int32_t y = UI_BOX_MARGIN_V;
	const struct ui_screen_info *screen = state->screen;
	const char *error_body = error_map[state->error_code].body;

	VB2_TRY(init_screen());

	/* If the screen is blank, turn off the backlight; else turn it on. */
	backlight_update(screen->id != VB2_SCREEN_BLANK);

	/*
	 * Dim the screen.  Basically, if we're going to show a
	 * dialog, we need to dim the background colors so it's not so
	 * distracting.
	 */
	if (error_body != NULL)
		set_blend(&ui_color_black, ALPHA(60));

	if (screen->draw)
		rv = screen->draw(state, prev_state);
	else
		rv = ui_draw_default(state, prev_state);

	if (rv) {
		UI_ERROR("Drawing screen %#x failed: %#x\n", screen->id, rv);
		/* Print fallback message if drawing failed. */
		if (screen->mesg)
			ui_draw_textbox(screen->mesg, &y, 1);
	}
	/* Disable screen dimming. */
	if (error_body != NULL)
		clear_blend();
	/*
	 * If there's an error message to be printed, print it out.
	 * If we're already printing out a fallback message, give it
	 * priority and don't print out more error messages.  Also,
	 * print out the error message to the AP console.
	 */
	if (!rv &&
	    state->error_code != VB2_UI_ERROR_NONE &&
	    error_body != NULL) {
		print_error_message(error_body, state->locale->code);
		UI_ERROR("%s\n", error_body);
	}

	return rv;
}

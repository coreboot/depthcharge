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
		ui_print_fallback_message(desc->mesg);

	return rv;
}

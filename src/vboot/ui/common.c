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

struct ui_error {
	/* File name of error strings. */
	const char *file;
	/* Whether to show dev mode URL below the error strings. */
	int show_dev_url;
	/* Fallback message */
	const char *mesg;
};

static const struct ui_error errors[] = {
	[VB2_UI_ERROR_DEV_MODE_ALREADY_ENABLED] = {
		.file = "error_dev_mode_enabled.bmp",
		.mesg = "Developer mode is already turned on.",
	},
	[VB2_UI_ERROR_UNTRUSTED_CONFIRMATION] = {
		.file = "error_untrusted_confirm.bmp",
		.mesg = "You cannot use an external keyboard to turn on\n"
			"developer mode. Please use the on-device buttons\n"
			"noted in the navigation instructions.",
	},
	[VB2_UI_ERROR_TO_NORM_NOT_ALLOWED] = {
		.file = "error_to_norm_not_allowed.bmp",
		.mesg = "Returning to secure mode disallowed by GBB flags.",
	},
	[VB2_UI_ERROR_EXTERNAL_BOOT_DISABLED] = {
		.file = "error_ext_boot_disabled.bmp",
		.show_dev_url = 1,
		.mesg = "External boot is disabled. For more information,\n"
			"see: google.com/chromeos/devmode",
	},
	[VB2_UI_ERROR_ALTFW_DISABLED] = {
		.file = "error_alt_boot_disabled.bmp",
		.show_dev_url = 1,
		.mesg = "Alternate bootloaders are disabled. For more\n"
			"information, see: google.com/chromeos/devmode",
	},
	[VB2_UI_ERROR_ALTFW_EMPTY] = {
		.file = "error_no_alt_bootloader.bmp",
		.show_dev_url = 1,
		.mesg = "Could not find an alternate bootloader. To learn how\n"
			"to install one, see: google.com/chromeos/devmode",
	},
	[VB2_UI_ERROR_ALTFW_FAILED] = {
		.file = "error_alt_boot_failed.bmp",
		.mesg = "Something went wrong launching the alternate\n"
			"bootloader. View firmware log for details.",
	},
	[VB2_UI_ERROR_DEBUG_LOG] = {
		.file = "error_debug_info.bmp",
		.mesg = "Could not get debug info.",
	},
	[VB2_UI_ERROR_FIRMWARE_LOG] = {
		.file = "error_firmware_log.bmp",
		.mesg = "Could not get firmware log.",
	},
	[VB2_UI_ERROR_DIAGNOSTICS] = {
		.file = "error_diagnostics.bmp",
		.mesg = "Could not get diagnostic information.",
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

	enable_graphics_buffer();
	backlight_update(1);

	initialized = 1;
	return VB2_SUCCESS;
}

static vb2_error_t show_error_box(const struct ui_error *error,
				  const struct ui_locale *locale)
{
	vb2_error_t rv = VB2_SUCCESS;
	int32_t x, y;
	const int32_t w = UI_SIZE_AUTO;
	int32_t h;
	int32_t button_width;
	const uint32_t flags = PIVOT_H_LEFT | PIVOT_V_TOP;
	const int reverse = locale->rtl;

	/* Center the box on the screen */
	x = (UI_SCALE - UI_ERROR_BOX_WIDTH) / 2;
	y = (UI_SCALE - UI_ERROR_BOX_HEIGHT) / 2;

	/* Clear printing area */
	ui_draw_rounded_box(x, y,
			    UI_ERROR_BOX_WIDTH,
			    UI_ERROR_BOX_HEIGHT,
			    &ui_color_error_box,
			    0,
			    UI_ERROR_BOX_RADIUS,
			    reverse);

	x += UI_ERROR_BOX_PADDING;
	y += UI_ERROR_BOX_PADDING;

	/* Insert icon */
	struct ui_bitmap bitmap;
	VB2_TRY(ui_get_bitmap("ic_info.bmp", NULL, 0, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y,
			       UI_ERROR_BOX_ICON_HEIGHT,
			       UI_ERROR_BOX_ICON_HEIGHT,
			       flags, reverse));

	/* Insert in the body */
	y += UI_ERROR_BOX_SECTION_SPACING + UI_ERROR_BOX_ICON_HEIGHT;
	VB2_TRY(ui_get_bitmap(error->file, locale->code, 0, &bitmap));
	h = UI_ERROR_BOX_TEXT_HEIGHT * ui_get_bitmap_num_lines(&bitmap);
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, h, flags, reverse));
	y += h;
	if (error->show_dev_url) {
		y += UI_ERROR_BOX_TEXT_LINE_SPACING;
		VB2_TRY(ui_get_bitmap("dev_mode_url.bmp", NULL, 0, &bitmap));
		h = UI_ERROR_BOX_TEXT_HEIGHT * ui_get_bitmap_num_lines(&bitmap);
		VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, h, flags, reverse));
		y += h;
	}

	/* Insert "Back" button */
	const struct ui_menu_item back_item = {
		.file = "btn_back.bmp",
	};
	VB2_TRY(ui_get_bitmap(back_item.file, locale->code, 0, &bitmap));
	int32_t text_width;
	VB2_TRY(ui_get_bitmap_width(&bitmap, UI_BUTTON_TEXT_HEIGHT,
				    &text_width));

	button_width = text_width + (UI_BUTTON_TEXT_PADDING_H * 2);
	/* x and y are top-left corner of the button */
	x = (UI_SCALE + UI_ERROR_BOX_WIDTH) / 2 -
		UI_ERROR_BOX_PADDING - button_width;
	y = (UI_SCALE + UI_ERROR_BOX_HEIGHT) / 2 -
		UI_ERROR_BOX_PADDING - UI_BUTTON_HEIGHT;
	VB2_TRY(ui_draw_button(&back_item,
			       locale->code,
			       x, y,
			       button_width,
			       UI_BUTTON_HEIGHT,
			       reverse, 1, 0, 0));

	return rv;
}

static const struct rgb_color colors[] = {
	[0x0] = { 0xff, 0xc0, 0xcb },	/* pink */
	[0x1] = { 0xff, 0x00, 0x00 },	/* red */
	[0x2] = { 0xff, 0xa5, 0x00 },	/* orange */
	[0x3] = { 0xff, 0xff, 0x00 },	/* yellow */
	[0x4] = { 0xa5, 0x2a, 0x2a },	/* brown */
	[0x5] = { 0x80, 0x00, 0x00 },	/* maroon */
	[0x6] = { 0x80, 0x80, 0x00 },	/* olive */
	[0x7] = { 0x00, 0xff, 0x00 },	/* lime */
	[0x8] = { 0x90, 0xee, 0x90 },	/* light green */
	[0x9] = { 0x00, 0x80, 0x00 },	/* green */
	[0xa] = { 0x00, 0xff, 0xff },	/* cyan */
	[0xb] = { 0x00, 0x80, 0x80 },	/* teal */
	[0xc] = { 0x00, 0x00, 0xff },	/* blue */
	[0xd] = { 0x00, 0x00, 0x80 },	/* navy */
	[0xe] = { 0xff, 0x00, 0xff },	/* magenta */
	[0xf] = { 0x80, 0x00, 0x80 },	/* purple */
};

/*
 * When we don't know how much of the drawing failed, draw colored stripes as a
 * fallback so the screen can be identified in a pinch. Place the stripes at the
 * very top of the screen to avoid covering up anything that was drawn
 * successfully.
 */
static void draw_fallback_stripes(enum vb2_screen screen,
				  uint32_t selected_item)
{
	int i, shift;
	int32_t x, y;
	const int32_t h = UI_FALLBACK_STRIPE_HEIGHT;
	uint32_t digit;

	/* stripe 1: reference color bar */
	y = 0;
	x = 0;
	for (i = 0; i < ARRAY_SIZE(colors); i++) {
		ui_draw_box(x, y, h, h, &colors[i], 0);
		x += h;
	}

	/* stripe 2: screen id in hex encoding */
	y += h;
	x = 0;
	for (shift = 3; shift >= 0; shift--) {  // Only display 4 digits
		digit = (screen >> (shift * 4)) & 0xf;
		ui_draw_box(x, y, h, h, &colors[digit], 0);
		x += h;
	}

	/* stripe 3: selected_item by position */
	y += h;
	ui_draw_box(0, y, h * ARRAY_SIZE(colors), h, &colors[0xd], 0);
	if (selected_item >= ARRAY_SIZE(colors))
		return;
	x = h * selected_item;
	ui_draw_box(x, y, h, h, &colors[0x0], 0);
}

vb2_error_t ui_display_screen(struct ui_state *state,
			      const struct ui_state *prev_state)
{
	vb2_error_t rv;
	int32_t y = UI_BOX_MARGIN_V;
	const struct ui_screen_info *screen = state->screen;
	const struct ui_error *error = NULL;

	VB2_TRY(init_screen());

	if (state->error_code != VB2_UI_ERROR_NONE)
		error = &errors[state->error_code];

	/*
	 * Dim the screen.  Basically, if we're going to show a
	 * dialog, we need to dim the background colors so it's not so
	 * distracting.
	 */
	if (error)
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
		/* Also draw colored stripes */
		draw_fallback_stripes(screen->id, state->selected_item);
	}
	/* Disable screen dimming. */
	if (error)
		clear_blend();
	/*
	 * If there's an error message to be printed, print it out.
	 * If we're already printing out a fallback message, give it
	 * priority and don't show the error box. Also, print out the
	 * error message to the AP console.
	 */
	if (rv == VB2_SUCCESS && error) {
		show_error_box(error, state->locale);
		if (error->mesg)
			UI_WARN("%s\n", error->mesg);
	}

	return rv;
}

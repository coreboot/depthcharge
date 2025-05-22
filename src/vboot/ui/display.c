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
#include <stdbool.h>
#include <vb2_api.h>

#include "drivers/ec/cros/ec.h"
#include "drivers/video/display.h"
#include "vboot/ui.h"

struct ui_error_message {
	/* File name of error strings. */
	const char *file;
	/* Whether to show dev mode URL below the error strings. */
	int show_dev_url;
	/* Fallback message */
	const char *mesg;
};

#define DEV_URL "google.com/chromeos/devmode"

static const struct ui_error_message errors[] = {
	[UI_ERROR_MINIOS_BOOT_FAILED] = {
		.file = "error_internet_recovery.bmp",
		.mesg = "Internet recovery partition corrupted or missing.\n"
			"Please recover using external storage instead.",
	},
	[UI_ERROR_DEV_MODE_ALREADY_ENABLED] = {
		.file = "error_dev_mode_enabled.bmp",
		.mesg = "Developer mode is already turned on.",
	},
	[UI_ERROR_UNTRUSTED_CONFIRMATION] = {
		.file = "error_untrusted_confirm.bmp",
		.mesg = "You cannot use an external keyboard to turn on\n"
			"developer mode. Please use the on-device buttons\n"
			"noted in the navigation instructions.",
	},
	[UI_ERROR_TO_NORM_NOT_ALLOWED] = {
		.file = "error_to_norm_not_allowed.bmp",
		.mesg = "Returning to secure mode disallowed by GBB flags.",
	},
	/* TODO(b/210875258): Create google.com/chromeos/blocked_devmode */
	[UI_ERROR_DEV_BOOT_NOT_ALLOWED] = {
		.file = "error_dev_boot_not_allowed.bmp",
		.show_dev_url = 1,
		.mesg = "Booting in developer mode is not allowed. For more\n"
			"info, visit: " DEV_URL,
	},
	[UI_ERROR_INTERNAL_BOOT_FAILED] = {
		.file = "error_int_boot_failed.bmp",
		.mesg = "Something went wrong booting from internal disk.\n"
			"View firmware log for details.",
	},
	[UI_ERROR_EXTERNAL_BOOT_DISABLED] = {
		.file = "error_ext_boot_disabled.bmp",
		.show_dev_url = 1,
		.mesg = "Booting from an external disk is disabled. For more\n"
			"info, visit: " DEV_URL,
	},
	[UI_ERROR_ALTFW_DISABLED] = {
		.file = "error_alt_boot_disabled.bmp",
		.show_dev_url = 1,
		.mesg = "Alternate bootloaders are disabled. For more info\n"
			"visit: " DEV_URL,
	},
	[UI_ERROR_ALTFW_EMPTY] = {
		.file = "error_no_alt_bootloader.bmp",
		.show_dev_url = 1,
		.mesg = "Could not find an alternate bootloader. To learn how\n"
			"to install one, visit: " DEV_URL,
	},
	[UI_ERROR_ALTFW_FAILED] = {
		.file = "error_alt_boot_failed.bmp",
		.mesg = "Something went wrong launching the alternate\n"
			"bootloader. View firmware log for details.",
	},
	[UI_ERROR_DEBUG_LOG] = {
		.file = "error_debug_info.bmp",
		.mesg = "Could not get debug info.",
	},
	[UI_ERROR_FIRMWARE_LOG] = {
		.file = "error_firmware_log.bmp",
		.mesg = "Could not get firmware log.",
	},
	[UI_ERROR_DIAGNOSTICS] = {
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
	clear_screen(&ui_color_black);
	if (display_init())
		return VB2_ERROR_UI_DISPLAY_INIT;

	enable_graphics_buffer();
	backlight_update(true);

	initialized = 1;
	return VB2_SUCCESS;
}

static vb2_error_t show_error_box(const struct ui_error_message *error,
				  const struct ui_state *state)
{
	vb2_error_t rv = VB2_SUCCESS;
	int32_t x, y;
	const int32_t w = UI_SIZE_AUTO;
	int32_t h;
	int32_t button_width;
	const uint32_t flags = PIVOT_H_LEFT | PIVOT_V_TOP;
	const int reverse = state->locale->rtl;
	const char *locale_code = state->locale->code;

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
	VB2_TRY(ui_get_bitmap(error->file, locale_code, 0, &bitmap));
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
	VB2_TRY(ui_get_bitmap(back_item.file, locale_code, 0, &bitmap));
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
			       state,
			       x, y,
			       button_width,
			       UI_BUTTON_HEIGHT,
			       1, 0, 0));

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
static void draw_fallback_stripes(enum ui_screen screen,
				  uint32_t focused_item)
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

	/* stripe 3: focused_item by position */
	y += h;
	ui_draw_box(0, y, h * ARRAY_SIZE(colors), h, &colors[0xd], 0);
	if (focused_item >= ARRAY_SIZE(colors))
		return;
	x = h * focused_item;
	ui_draw_box(x, y, h, h, &colors[0x0], 0);
}

/*
 * Calculate the 32-bit value to report as the AP firmware state
 *
 * This must be stable for all time, since FAFT tests rely on it.
 *
 * @param state	UI state to report
 * @return corresponding 32-bit value
 */
static uint32_t calc_ap_fw_state(const struct ui_state *state)
{
	/*
	 * For now we only report the screen ID. At some point the focused_item
	 * could be added, but we may want to renumber the screens to take up
	 * less space, first.
	 *
	 * Current valid values are defined by enum ui_screen and currently use
	 * 11 bits.
	 */
	return state->screen->id;
}

vb2_error_t ui_display(struct ui_context *ui,
		       const struct ui_state *prev_state)
{
	vb2_error_t rv;
	const struct ui_state *state = ui->state;
	UI_INFO("screen=%#x, locale=%u, focused_item=%u, "
		"disabled_item_mask=%#x, hidden_item_mask=%#x, "
		"timer_disabled=%d, current_page=%u, error=%#x\n",
		state->screen->id, ui->state->locale->id, state->focused_item,
		state->disabled_item_mask, state->hidden_item_mask,
		state->timer_disabled, state->current_page, state->error_code);

	int32_t y = UI_BOX_MARGIN_V;
	const struct ui_screen_info *screen = state->screen;
	const struct ui_error_message *error = NULL;

	/*
	 * Tell the EC about our state...ignore errors since some ECs won't
	 * support this.
	 */
	if (CONFIG(DRIVER_EC_CROS))
		cros_ec_set_ap_fw_state(calc_ap_fw_state(ui->state));

	VB2_TRY(init_screen());

	if (state->error_code != UI_ERROR_NONE)
		error = &errors[state->error_code];

	/*
	 * Dim the screen.  Basically, if we're going to show a
	 * dialog, we need to dim the background colors so it's not so
	 * distracting.
	 */
	if (error)
		set_blend(&ui_color_black, ALPHA(60));

	if (screen->draw)
		rv = screen->draw(ui, prev_state);
	else
		rv = ui_draw_default(ui, prev_state);

	if (rv) {
		UI_ERROR("Drawing screen %#x failed: %#x\n", screen->id, rv);
		/* Print fallback message if drawing failed. */
		if (screen->mesg)
			ui_draw_textbox(screen->mesg, &y, 1);
		/* Also draw colored stripes */
		draw_fallback_stripes(screen->id, state->focused_item);
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
		show_error_box(error, state);
		if (error->mesg)
			UI_WARN("%s\n", error->mesg);
	}

	flush_graphics_buffer();

	return rv;
}

int ui_display_clear(void)
{
	disable_graphics_buffer();
	return clear_screen(&ui_color_black);
}

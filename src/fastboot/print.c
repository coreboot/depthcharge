/*
 * Copyright 2015 Google Inc.
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

#include "base/graphics.h"
#include "drivers/video/coreboot_fb.h"
#include "drivers/video/display.h"
#include "fastboot/print.h"

#define FB_MESSAGE_NORM_FG	15	/* white */
#define FB_MESSAGE_NORM_BG	0	/* black */
#define FB_MESSAGE_WARN_FG	11	/* yellow */
#define FB_MESSAGE_WARN_BG	0	/* black */
#define FB_MESSAGE_ERR_FG	1	/* red */
#define FB_MESSAGE_ERR_BG	0	/* black */

static const struct {
	int fg;
	int bg;
} color_table[] = {
	[PRINT_INFO] = {FB_MESSAGE_NORM_FG, FB_MESSAGE_NORM_BG},
	[PRINT_WARN] = {FB_MESSAGE_WARN_FG, FB_MESSAGE_WARN_BG},
	[PRINT_ERR]  = {FB_MESSAGE_ERR_FG,  FB_MESSAGE_ERR_BG},
};

/*
 * This function is used to print text at the center of screen. Depending on the
 * message type, different foreground and background colors are used.
 */
void fb_print_text_on_screen(fb_msg_t type, const char *msg, size_t len)
{
	static int initialized;

	if (initialized == 0) {
		if (graphics_init())
			return;

		backlight_update(1);
		initialized = 1;
	}

	/* Print text on black screen. */
	const struct rgb_color black = { 0x0, 0x0, 0x0 };

	graphics_print_single_text_block(msg, &black, color_table[type].fg,
			    color_table[type].bg, VIDEO_PRINTF_ALIGN_CENTER);

}

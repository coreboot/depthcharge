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

#include <assert.h>

#include "base/graphics.h"
#include "drivers/video/display.h"

int graphics_init(void)
{
	static int initialized = 0;

	if (initialized)
		return 0;

	if (display_init())
		return -1;

	/* initialize video console */
	video_init();
	video_console_clear();
	video_console_cursor_enable(0);

	backlight_update(1);

	initialized = 1;

	return 0;
}

int graphics_print_single_text_block(const char *msg,
				     const struct rgb_color *clr_scr_params,
				     int fg, int bg,
				     enum video_printf_align align)
{
	assert(msg);
	assert(clr_scr_params);

	if (clear_screen(clr_scr_params)) {
		printf("ERROR: Failed to clear screen\n");
		return -1;
	}

	unsigned int rows, cols;

	video_get_rows_cols(&rows, &cols);
	video_console_set_cursor(0, rows/2);
	video_printf(fg, bg, align, msg);

	return 0;
}

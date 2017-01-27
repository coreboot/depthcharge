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

#ifndef __BASE_GRAPHICS_H__
#define __BASE_GRAPHICS_H__

#include <libpayload.h>

/*
 * graphics_init performs following steps:
 * 1. Peform display and video intiailization.
 * 2. Clear console and enable cursor.
 * 3. Enable backlight.
 *
 * Returns 0 on success and -1 on error.
 */
int graphics_init(void);

/*
 * Print single block of text on center row of screen using provided parameters:
 * @msg - msg to be printed.
 * @clr_scrn_params - Provides background color for clearing screen
 * @fg - Foreground color for the message.
 * @bg - Background color for the message.
 * @align - Horizontal alignment for the message, vertical alignment is fixed
 * i.e. text is always printed on the center row. Value should be one of enum
 * video_printf_align.
 *
 * Returns 0 on success and -1 on error.
 */
int graphics_print_single_text_block(const char *msg,
				     const struct rgb_color *clr_scr_params,
				     int fg, int bg,
				     enum video_printf_align align);

/*
 * Print text at x,y coordinate of screen using provided parameters:
 * @msg - msg to be printed.
 * @fg - Foreground color for the message.
 * @bg - Background color for the message.
 * @x - x coordinate to print message
 * @y - y coordinate to print message
 * @align - Horizontal alignment for the message, vertical alignment is fixed
 * i.e. text is always printed on the center row. Value should be one of enum
 * video_printf_align.
 *
 * Returns 0 on success and -1 on error.
 */
int graphics_print_text_xy(const char *msg, int fg, int bg, int x, int y,
			   enum video_printf_align align);

#endif /* __BASE_GRAPHICS_H__ */

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

#ifndef __VBOOT_SCREENS_H__
#define __VBOOT_SCREENS_H__

#include <libpayload.h>
#include <stdint.h>
#include <vboot_api.h>

static const struct rgb_color color_pink	= { 0xff, 0x00, 0xff };
static const struct rgb_color color_yellow	= { 0xff, 0xff, 0x00 };
static const struct rgb_color color_orange	= { 0xff, 0x99, 0x00 };
static const struct rgb_color color_violet	= { 0x88, 0x00, 0x88 };
static const struct rgb_color color_green	= { 0x00, 0xff, 0x00 };
static const struct rgb_color color_blue	= { 0x00, 0x00, 0xff };
static const struct rgb_color color_red		= { 0xff, 0x00, 0x00 };
static const struct rgb_color color_brown	= { 0x66, 0x33, 0x00 };
static const struct rgb_color color_teal	= { 0x00, 0x99, 0x99 };
static const struct rgb_color color_light_blue	= { 0x00, 0x99, 0xff };
static const struct rgb_color color_grey	= { 0x88, 0x88, 0x88 };
static const struct rgb_color color_white	= { 0xff, 0xff, 0xff };
static const struct rgb_color color_black	= { 0x00, 0x00, 0x00 };

int vboot_draw_screen(uint32_t screen, uint32_t locale,
		      const VbScreenData *data);
int vboot_draw_ui(uint32_t screen, uint32_t locale,
		  uint32_t selected_index, uint32_t disabled_idx_mask,
		  uint32_t redraw_base);

/**
 * Print a string on the console using the standard font. The string buffer is
 * consumed by this operation and should not be reused after.
 *
 * @str:	String to print
 */
VbError_t vboot_print_string(char *str);

/**
 * Return number of supported locales
 *
 * @return number of supported locales
 */
int vboot_get_locale_count(void);

#endif /* __VBOOT_SCREENS_H__ */

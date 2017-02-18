/*
 * Copyright 2016 Google Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_HID_VIRTUAL_KEYBOARD_H__
#define __DRIVERS_HID_VIRTUAL_KEYBOARD_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/hid/i2c-hid.h"

#define KEY_POS(axis_x, axis_y, range_x, range_y) { \
	.x_min = axis_x - range_x, .x_max = axis_x + range_x, \
	.y_min = axis_y - range_y, .y_max = axis_y + range_y }

struct key_position {
	int x_min;
	int x_max;
	int y_min;
	int y_max;
};

struct key_array_t {
	struct key_position key[2];
	uint8_t contact_cnt;
	int key_code;
};

int vkb_havekey(i2chiddev_t *dev);
int vkb_getchar(void);
int configure_virtual_keyboard(i2chiddev_t *dev);
int board_virtual_keyboard_layout(struct key_array_t *board_key_array);
#endif

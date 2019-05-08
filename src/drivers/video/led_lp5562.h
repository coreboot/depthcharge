/*
 * Copyright (C) 2018 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SRC_DRIVERS_VIDEO_LED_LP5562__H__
#define __SRC_DRIVERS_VIDEO_LED_LP5562__H__

#include <vboot_api.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/video/display.h"

typedef struct {
	DisplayOps lp5562_display_ops;
	I2cOps     *lp5562_i2c_ops;
	uint8_t	   lp5562_base_addr;
} Lp5562DisplayOps;


/*
 * This driver allows to load programs into the LED controller. Usually the
 * program generates a certain LED blinking pattern. Usually CPU communicates
 * with the LED controller over i2c.
 */

/*
 * new_lp5562_display
 *
 * Create LP5562 display control strunctures. Input parameters are a pointer
 * to the i2c interface and the base address of the lp5562 used.
 *
 * Returns a pointer to DisplayOps structure.
 */
DisplayOps *new_led_lp5562_display(I2cOps *i2cOps, uint8_t base_addr);

#endif

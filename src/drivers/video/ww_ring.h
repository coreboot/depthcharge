/*
 * Copyright (C) 2015 Google, Inc.
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

#ifndef __SRC_DRIVERS_VIDEO_WW_RING__H__
#define __SRC_DRIVERS_VIDEO_WW_RING__H__

#include <vboot_api.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/video/display.h"

typedef struct {
	DisplayOps wwr_display_ops;
	I2cOps     *wwr_i2c_ops;
	uint8_t	   wwr_base_addr;
} WwRingDisplayOps;

/*
 * This driver allows to load programs into the LED controller. Usually the
 * program generates a certain LED blinking pattern. Usually CPU communicates
 * with the LED controller over i2c.
 */

/*
 * new_ww_ring_display
 *
 * Create WW ring display control strunctures. Input paramters are a pointer
 * to the i2c interface and the base address of the lp55231 used on the ring.
 *
 * Returns a pointer to DisplayOps structure.
 */
DisplayOps *new_ww_ring_display(I2cOps *i2cOps, uint8_t base_addr);

#endif

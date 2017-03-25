/*
 * Copyright 2017 Google Inc.
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

#ifndef __ARCTIC_SAND_BACKLIGHT_H
#define __ARCTIC_SAND_BACKLIGHT_H

#include "drivers/bus/i2c/i2c.h"
#include "drivers/gpio/gpio.h"

typedef struct {
	I2cOps     *i2c_ops;
	GpioOps    gpio_ops;
	uint8_t	   chip_addr;
	uint8_t	   initialized;
} ArcticSandBacklight;

GpioOps *new_arctic_sand_backlight(I2cOps *i2c_ops, uint8_t chip_addr);

#endif

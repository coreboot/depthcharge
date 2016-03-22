/*
 * Copyright 2015 Marvell Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ARMADA38X_I2C_H
#define __ARMADA38X_I2C_H

#include "drivers/bus/i2c/i2c.h"

typedef struct {
	I2cOps ops;
	u8 bus_num;
	u32 tclk;
	u32 base;
	unsigned initialized;
} Armada38xI2c;

Armada38xI2c *new_armada38x_i2c(u32 base, u32 tclk, u8 bus_num);

#endif /* __ARMADA38X_I2C_H */

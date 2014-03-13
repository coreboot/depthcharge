/*
 * Copyright 2013 Google Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA, 02110-1301 USA
 */

#ifndef __DRIVERS_BUS_I2C_EXYNOS5_USI_H__
#define __DRIVERS_BUS_I2C_EXYNOS5_USI_H__

#include "drivers/bus/i2c/i2c.h"

typedef struct Exynos5UsiI2c
{
	I2cOps ops;
	void *reg_addr;
	unsigned frequency;
	int ready;
} Exynos5UsiI2c;

Exynos5UsiI2c *new_exynos5_usi_i2c(uintptr_t reg_addr, unsigned frequency);

#endif /* __DRIVERS_BUS_I2C_EXYNOS5_USI_H__ */

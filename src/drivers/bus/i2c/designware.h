/*
 * (C) Copyright 2009
 * Vipin Kumar, ST Micoelectronics, vipin.kumar@st.com.
 * Copyright 2013 Google LLC
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_BUS_I2C_DESIGNWARE_H__
#define __DRIVERS_BUS_I2C_DESIGNWARE_H__

#include <stdint.h>

#include "drivers/bus/i2c/i2c.h"

typedef struct DesignwareI2c
{
	I2cOps ops;
	void *regs;
	int speed;
	int clk_mhz;
	int initialized;
} DesignwareI2c;

DesignwareI2c *new_designware_i2c(uintptr_t regs, int speed, int clk_mhz);
DesignwareI2c *new_pci_designware_i2c(pcidev_t dev, int speed, int clk_mhz);

#endif /* __DRIVERS_BUS_I2C_DESIGNWARE_H__ */

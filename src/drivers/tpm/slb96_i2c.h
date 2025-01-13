/*
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
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_TPM_SLB96_I2C_H__
#define __DRIVERS_TPM_SLB96_I2C_H__

#include "drivers/tpm/i2c.h"

enum i2c_chip_type {
	SLB9635,
	SLB9645,
	UNKNOWN,
};

typedef struct Slb96I2c
{
	I2cTpm base;
	uint8_t buf[sizeof(uint8_t) + TpmMaxBufSize]; // addr + buff size
	enum i2c_chip_type chip_type;
} Slb96I2c;

Slb96I2c *new_slb96_i2c(I2cOps *bus, uint8_t addr);

#endif /* __DRIVERS_TPM_SLB96_I2C_H__ */

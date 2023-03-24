/*
 * Copyright 2016 Google LLC
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

#ifndef __DRIVERS_TPM_GOOGLE_I2C_H__
#define __DRIVERS_TPM_GOOGLE_I2C_H__

#include "drivers/tpm/i2c.h"

#define GSC_I2C_ADDR	0x50

enum {
	GscMaxBufSize = 63,
};

typedef int (*gsc_irq_status_t)(void);

typedef struct GscI2c
{
	I2cTpm base;
	gsc_irq_status_t irq_status;
	uint8_t buf[sizeof(uint8_t) + GscMaxBufSize];
} GscI2c;

GscI2c *new_gsc_i2c(I2cOps *bus, uint8_t addr, gsc_irq_status_t irq_status);

#endif /* __DRIVERS_TPM_GOOGLE_I2C_H__ */

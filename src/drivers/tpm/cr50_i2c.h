/*
 * Copyright 2016 Google Inc.
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

#ifndef __DRIVERS_TPM_CR50_I2C_H__
#define __DRIVERS_TPM_CR50_I2C_H__

#include "drivers/tpm/i2c.h"

enum {
	Cr50MaxBufSize = 63,
};

typedef int (*cr50_irq_status_t)(void);

typedef struct Cr50I2c
{
	I2cTpm base;
	cr50_irq_status_t irq_status;
	uint8_t buf[sizeof(uint8_t) + Cr50MaxBufSize];
} Cr50I2c;

Cr50I2c *new_cr50_i2c(I2cOps *bus, uint8_t addr, cr50_irq_status_t irq_status);

#endif /* __DRIVERS_TPM_CR50_I2C_H__ */

/*
 * Copyright (C) 2011 Infineon Technologies
 * Copyright 2013 Google Inc.
 *
 * Authors:
 * Peter Huewe <huewe.external@infineon.com>
 *
 * Version: 2.1.1
 *
 * Description:
 * Device driver for TCG/TCPA TPM (trusted platform module).
 * Specifications at www.trustedcomputinggroup.org
 *
 * It is based on the Linux kernel driver tpm.c from Leendert van
 * Dorn, Dave Safford, Reiner Sailer, and Kyleen Hall.
 *
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __DRIVERS_TPM_I2C_H__
#define __DRIVERS_TPM_I2C_H__

#include <stdint.h>

#include "base/cleanup_funcs.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/tpm/tpm.h"

typedef struct I2cTpmChipOps
{
	int (*init)(struct I2cTpmChipOps *me);
	int (*cleanup)(struct I2cTpmChipOps *me);

	int (*recv)(struct I2cTpmChipOps *me, uint8_t *, size_t);
	int (*send)(struct I2cTpmChipOps *me, const uint8_t *, size_t);
	void (*cancel)(struct I2cTpmChipOps *me);
	uint8_t (*status)(struct I2cTpmChipOps *me);
} I2cTpmChipOps;

typedef struct I2cTpm
{
	TpmOps ops;
	I2cTpmChipOps chip_ops;

	I2cOps *bus;
	uint8_t addr;

	uint8_t req_complete_mask;
	uint8_t req_complete_val;
	uint8_t req_canceled;
	int locality;

	int initialized;
	CleanupFunc cleanup;
} I2cTpm;

void i2ctpm_fill_in(I2cTpm *tpm, I2cOps *bus, uint8_t addr,
		    uint8_t req_complete_mask, uint8_t req_complete_val,
		    uint8_t req_canceled);

#endif /* __DRIVERS_TPM_I2C_H__ */

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

#ifndef __SRC_DRIVERS_TPM_TPM_SPI_H
#define __SRC_DRIVERS_TPM_TPM_SPI_H

#include <base/cleanup_funcs.h>
#include <drivers/bus/spi/spi.h>
#include <drivers/tpm/tpm.h>

// Supported TPM device types.
typedef enum {
	CR50_TPM,
	UNKNOWN_SPI_TPM,  // This would trigger default behavior.
} SpiTpmType;

typedef struct
{
	TpmOps ops;
	SpiOps *bus;
	CleanupFunc cleanup;
	SpiTpmType chip_type;
} SpiTpm;

SpiTpm *new_tpm_spi(SpiOps *bus);

#endif // __SRC_DRIVERS_TPM_TPM_SPI_H

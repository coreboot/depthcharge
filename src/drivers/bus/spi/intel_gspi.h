/*
 * Copyright 2017 Google Inc. All rights reserved.
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

#ifndef __DRIVERS_BUS_SPI_INTEL_GSPI_H__
#define __DRIVERS_BUS_SPI_INTEL_GSPI_H__

#include <libpayload.h>

#include "drivers/bus/spi/spi.h"

enum spi_polarity {
	SPI_POLARITY_LOW,
	SPI_POLARITY_HIGH,
};

enum spi_clk_phase {
	SPI_CLOCK_PHASE_FIRST,
	SPI_CLOCK_PHASE_SECOND,
};

typedef struct intel_gspi_setup_params {
	pcidev_t dev;
	enum spi_polarity cs_polarity;
	enum spi_clk_phase clk_phase;
	enum spi_polarity clk_polarity;
	uint32_t ref_clk_mhz;
	uint32_t gspi_clk_mhz;
} IntelGspiSetupParams;

SpiOps *new_intel_gspi(const IntelGspiSetupParams *p);

#endif /* __DRIVERS_BUS_SPI_INTEL_GSPI_H__ */

/*
 * Copyright 2015 Marvell Inc.
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

#ifndef __ARMADA38X_SPI_H__
#define __ARMADA38X_SPI_H__

#include "drivers/flash/spi.h"
#include "drivers/bus/spi/spi.h"

struct spi_slave {
	unsigned int bus;
	unsigned int cs;
};

typedef struct {
	struct spi_slave slave;
	unsigned int base;
	unsigned int tclk;
	unsigned int max_hz;
	unsigned int bus_started;
	int type;
} Armada38xSpiSlave;

typedef struct {
	SpiOps ops;
	Armada38xSpiSlave armada38x_spi_slave;
} SpiController;

SpiController *new_armada38x_spi(unsigned int base,
				unsigned int tclk,
				unsigned int bus_num,
				unsigned int cs,
				unsigned int max_hz);

#endif /* __ARMADA38X_SPI_H__ */

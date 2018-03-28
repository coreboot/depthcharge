/*
 * Copyright 2018 Google LLC
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


#ifndef __DRIVERS_BUS_SPI_BITBANG_H__
#define __DRIVERS_BUS_SPI_BITBANG_H__

#include "drivers/bus/spi/spi.h"
#include "drivers/gpio/gpio.h"

typedef struct BitbangSpi {
	SpiOps ops;
	GpioOps *miso;
	GpioOps *mosi;
	GpioOps *clk;
	GpioOps *cs;
} BitbangSpi;

BitbangSpi *new_bitbang_spi(GpioOps *miso, GpioOps *mosi,
			    GpioOps *clk, GpioOps *cs);


#endif	/* __DRIVERS_BUS_SPI_BITBANG_H__ */

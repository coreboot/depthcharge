/*
 * Chromium OS EC driver - SPI interface
 *
 * Copyright 2013 Google Inc.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __DRIVERS_EC_CROS_SPI_H__
#define __DRIVERS_EC_CROS_SPI_H__

#include "drivers/bus/spi/spi.h"
#include "drivers/ec/cros/message.h"
#include "drivers/ec/cros/ec.h"

struct SpiOps;
typedef struct SpiOps SpiOps;

typedef struct
{
	CrosEcBusOps ops;
	SpiOps *spi;

	// Time the last transfer ended.
	uint64_t last_transfer;

	void *buf;
} CrosEcSpiBus;

enum {
	// response, arglen
	CROS_EC_SPI_IN_HDR_SIZE = 2,
	// version, cmd, arglen
	CROS_EC_SPI_OUT_HDR_SIZE = 3
};

CrosEcSpiBus *new_cros_ec_spi_bus(SpiOps *spi);

#endif /* __DRIVERS_EC_CROS_SPI_H__ */

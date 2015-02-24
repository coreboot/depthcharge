/*
 * Copyright (C) 2015 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __DRIVERS_BUS_SPI_BCM_QSPI_H__
#define __DRIVERS_BUS_SPI_BCM_QSPI_H__

#include "drivers/bus/spi/spi.h"

#define BCM_QSPI_BASE	0x18047000

/* SPI mode flags */
#define	SPI_CPHA	0x01		/* clock phase */
#define	SPI_CPOL	0x02		/* clock polarity */
#define	SPI_MODE_0	(0|0)		/* original MicroWire */
#define	SPI_MODE_1	(0|SPI_CPHA)
#define	SPI_MODE_2	(SPI_CPOL|0)
#define	SPI_MODE_3	(SPI_CPOL|SPI_CPHA)

typedef struct bcm_qspi {
	SpiOps ops;

	unsigned int max_hz;
	unsigned int spi_mode;

	int mspi_enabled;
	int mspi_16bit;

	int bus_started;

	/* Registers */
	void *reg;
} bcm_qspi;

bcm_qspi *new_bcm_qspi(uintptr_t reg, unsigned int spi_mode,
		       unsigned int max_hz);

#endif				/* __DRIVERS_BUS_SPI_BCM_QSPI_H__ */

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
 */

#include <libpayload.h>
#include "base/container_of.h"
#include "drivers/bus/spi/spi.h"
#include "drivers/bus/spi/bcm_qspi.h"

#define QSPI_CLK			100000000U
#define QSPI_WAIT_TIMEOUT_US		200000U

/* Controller attributes */
#define SPBR_MIN			8U
#define SPBR_MAX			255U
#define NUM_TXRAM			32
#define NUM_RXRAM			32
#define NUM_CDRAM			16

/*
 * Register fields
 */
#define MSPI_SPCR0_MSB_BITS_8		0x00000020

/* BSPI registers */
#define BSPI_MAST_N_BOOT_CTRL_REG	0x008
#define BSPI_BUSY_STATUS_REG		0x00c

/* MSPI registers */
#define MSPI_SPCR0_LSB_REG		0x200
#define MSPI_SPCR0_MSB_REG		0x204
#define MSPI_SPCR1_LSB_REG		0x208
#define MSPI_SPCR1_MSB_REG		0x20c
#define MSPI_NEWQP_REG			0x210
#define MSPI_ENDQP_REG			0x214
#define MSPI_SPCR2_REG			0x218
#define MSPI_STATUS_REG			0x220
#define MSPI_CPTQP_REG			0x224
#define MSPI_TXRAM_REG			0x240
#define MSPI_RXRAM_REG			0x2c0
#define MSPI_CDRAM_REG			0x340
#define MSPI_WRITE_LOCK_REG		0x380
#define MSPI_DISABLE_FLUSH_GEN_REG	0x384

/*
 * Register access macros
 */
#define REG_RD(x)	read32(x)
#define REG_WR(x, y)	write32((x), (y))
#define REG_CLR(x, y)	REG_WR((x), REG_RD(x) & ~(y))
#define REG_SET(x, y)	REG_WR((x), REG_RD(x) | (y))

#define RXRAM_16B(p, i)	(REG_RD((p)->reg + MSPI_RXRAM_REG + ((i) << 2)) & 0xff)
#define RXRAM_8B(p, i)	(REG_RD((p)->reg + MSPI_RXRAM_REG + \
				((((i) << 1) + 1) << 2)) & 0xff)

static int qspi_transfer(SpiOps *me, void *in, const void *out, uint32_t size)
{
	bcm_qspi *bus = container_of(me, bcm_qspi, ops);
	const u8 *tx = (const u8 *)out;
	u8 *rx = (u8 *)in;
	unsigned int chunk;
	unsigned int queues;
	unsigned int i;
	uint64_t start;

	if (!bus->bus_started)
		return -1;

	if (size & 1) {
		/* Use 8-bit queue for odd-bytes transfer */
		if (bus->mspi_16bit) {
			REG_SET(bus->reg + MSPI_SPCR0_MSB_REG,
				MSPI_SPCR0_MSB_BITS_8);
			bus->mspi_16bit = 0;
		}
	} else {
		/* Use 16-bit queue for even-bytes transfer */
		if (!bus->mspi_16bit) {
			REG_CLR(bus->reg + MSPI_SPCR0_MSB_REG,
				MSPI_SPCR0_MSB_BITS_8);
			bus->mspi_16bit = 1;
		}
	}

	while (size) {
		/* Separate code for 16bit and 8bit transfers for performance */
		if (bus->mspi_16bit) {
			/* Determine how many bytes to process this time */
			chunk = MIN(size, NUM_CDRAM * 2);
			queues = (chunk - 1) / 2 + 1;
			size -= chunk;

			/* Fill CDRAMs */
			for (i = 0; i < queues; i++)
				REG_WR(bus->reg + MSPI_CDRAM_REG + (i << 2),
				       0xc2);

			/* Fill TXRAMs */
			for (i = 0; i < chunk; i++) {
				REG_WR(bus->reg + MSPI_TXRAM_REG + (i << 2),
				       tx ? tx[i] : 0xff);
			}
		} else {
			/* Determine how many bytes to process this time */
			chunk = MIN(size, NUM_CDRAM);
			queues = chunk;
			size -= chunk;

			/* Fill CDRAMs and TXRAMS */
			for (i = 0; i < chunk; i++) {
				REG_WR(bus->reg + MSPI_CDRAM_REG + (i << 2),
				       0x82);
				REG_WR(bus->reg + MSPI_TXRAM_REG + (i << 3),
				       tx ? tx[i] : 0xff);
			}
		}

		/* Setup queue pointers */
		REG_WR(bus->reg + MSPI_NEWQP_REG, 0);
		REG_WR(bus->reg + MSPI_ENDQP_REG, queues - 1);

		/* Deassert CS */
		if (size == 0)
			REG_CLR(bus->reg + MSPI_CDRAM_REG +
				((queues - 1) << 2), 0x0);

		/* Kick off */
		REG_WR(bus->reg + MSPI_STATUS_REG, 0);
		REG_WR(bus->reg + MSPI_SPCR2_REG, 0xc0);	/* cont | spe */

		/* Wait for completion */
		start = timer_us(0);
		while (timer_us(start) < QSPI_WAIT_TIMEOUT_US) {
			if (REG_RD(bus->reg + MSPI_STATUS_REG) & 1)
				break;
		}
		if ((REG_RD(bus->reg + MSPI_STATUS_REG) & 1) == 0) {
			/* Make sure no operation is in progress */
			REG_WR(bus->reg + MSPI_SPCR2_REG, 0);
			udelay(1);
			return -1;
		}

		/* Read data */
		if (rx) {
			if (bus->mspi_16bit) {
				for (i = 0; i < chunk; i++)
					rx[i] = RXRAM_16B(bus, i);
			} else {
				for (i = 0; i < chunk; i++)
					rx[i] = RXRAM_8B(bus, i);
			}
		}

		/* Advance pointers */
		if (tx)
			tx += chunk;
		if (rx)
			rx += chunk;
	}

	return 0;
}

static int mspi_enable(struct bcm_qspi *bus)
{
	uint64_t start;

	/* Switch to MSPI if not yet */
	if ((REG_RD(bus->reg + BSPI_MAST_N_BOOT_CTRL_REG) & 1) == 0) {
		start = timer_us(0);
		while (timer_us(start) < QSPI_WAIT_TIMEOUT_US) {
			if ((REG_RD(bus->reg + BSPI_BUSY_STATUS_REG) & 1)
			    == 0) {
				REG_WR(bus->reg + BSPI_MAST_N_BOOT_CTRL_REG, 1);
				udelay(1);
				break;
			}
			udelay(1);
		}
		if ((REG_RD(bus->reg + BSPI_MAST_N_BOOT_CTRL_REG) & 1) != 1)
			return -1;
	}
	bus->mspi_enabled = 1;
	return 0;
}

static int qspi_start(SpiOps *me)
{
	bcm_qspi *bus = container_of(me, bcm_qspi, ops);

	if (bus->bus_started)
		return -1;

	if (!bus->mspi_enabled)
		if (mspi_enable(bus))
			return -1;

	/* MSPI: Enable write lock */
	REG_WR(bus->reg + MSPI_WRITE_LOCK_REG, 1);

	bus->bus_started = 1;

	return 0;
}

static int qspi_stop(SpiOps *me)
{
	bcm_qspi *bus = container_of(me, bcm_qspi, ops);

	/* MSPI: Disable write lock */
	REG_WR(bus->reg + MSPI_WRITE_LOCK_REG, 0);

	bus->bus_started = 0;

	return 0;
}

bcm_qspi *new_bcm_qspi(uintptr_t reg, unsigned int spi_mode,
		       unsigned int max_hz)
{
	unsigned int spbr;
	bcm_qspi *bus = xzalloc(sizeof(*bus));

	bus->reg = (void *)reg;
	bus->spi_mode = spi_mode;
	bus->max_hz = max_hz;

	bus->mspi_enabled = 0;
	bus->bus_started = 0;

	/* MSPI: Basic hardware initialization */
	REG_WR(bus->reg + MSPI_SPCR1_LSB_REG, 0);
	REG_WR(bus->reg + MSPI_SPCR1_MSB_REG, 0);
	REG_WR(bus->reg + MSPI_NEWQP_REG, 0);
	REG_WR(bus->reg + MSPI_ENDQP_REG, 0);
	REG_WR(bus->reg + MSPI_SPCR2_REG, 0);

	/* MSPI: SCK configuration */
	spbr = (QSPI_CLK - 1) / (2 * bus->max_hz) + 1;
	REG_WR(bus->reg + MSPI_SPCR0_LSB_REG,
	       MAX(MIN(spbr, SPBR_MAX), SPBR_MIN));

	/* MSPI: Mode configuration (8 bits by default) */
	bus->mspi_16bit = 0;
	REG_WR(bus->reg + MSPI_SPCR0_MSB_REG,
	       0x80 |			/* Master */
	       (8 << 2) |		/* 8 bits per word */
	       (bus->spi_mode & 3));	/* mode: CPOL / CPHA */

	bus->ops.start = &qspi_start;
	bus->ops.stop = &qspi_stop;
	bus->ops.transfer = &qspi_transfer;
	return bus;
}

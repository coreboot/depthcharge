/*
 * Copyright 2014 Rockchip Electronics Co., Ltd.
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

#include <assert.h>
#include <libpayload.h>
#include "base/container_of.h"
#include "drivers/bus/spi/spi.h"
#include "drivers/bus/spi/rockchip.h"

#define spi_info(x...) do {if (0) printf(x); } while (0)

#define spi_err(x...)	printf(x)
#define SPI_TIMEOUT_US  100000
typedef struct {
	u32 ctrlr0;
	u32 ctrlr1;
	u32 enr;
	u32 ser;
	u32 baudr;
	u32 txftlr;
	u32 rxftlr;
	u32 txflr;
	u32 rxflr;
	u32 sr;
	u32 ipr;
	u32 imr;
	u32 isr;
	u32 risr;
	u32 icr;
	u32 dmacr;
	u32 dmatdlr;
	u32 damrdlr;
	u32 reserved0[(0x400 - 0x48) / 4];
	u32 txdr[0x100];
	u32 rxdr[0x100];
} RkSpiRegs;

#define FIFO_DEPTH			32

//Control register 0
#define CR0_OP_MODE_BIT			20
#define CR0_TS_MODE_BIT			18
enum {
	DUPLEX = 0,
	TXONLY,
	RXONLY
};
#define SPI_TMOD_OFFSET			18
#define SPI_TMOD_MASK			0x3
#define SPI_TMOD_TR			0x00	// xmit & recv
#define SPI_TMOD_TO			0x01	// xmit only
#define SPI_TMOD_RO			0x02	// recv only
#define SPI_TMOD_RESV			0x03

#define CR0_HALFWORD_TS_BIT		13
#define CR0_SSN_DELAY_BIT		10
#define CR0_CLOCK_POLARITY_BIT		7
#define CR0_CLOCK_PHASE_BIT		6
#define CR0_FRAME_SIZE_BIT		0
enum {
	FRAME_SIZE_4BIT,
	FRAME_SIZE_8BIT,
	FRAME_SIZE_16BIT
};

// Control register 1
#define CR1_FRAME_NUM_MASK		0xffff

//Controller enable register
enum {
	CONTROLLER_DISABLE,
	CONTROLLER_ENABLE
};

// Bit fields in SR, 7 bits
#define SR_MASK	0x7f
#define SR_BUSY	(1 << 0)
#define SR_TF_FULL	(1 << 1)
#define SR_TF_EMPT	(1 << 2)
#define SR_RF_EMPT	(1 << 3)
#define SR_RF_FULL	(1 << 4)

//Slave select register
#define MAX_SLAVE	2

static int rockchip_spi_wait_till_not_busy(RkSpi *bus)
{
	unsigned int delay = 1000000;
	RkSpiRegs *regs = bus->reg_addr;

	while (delay--) {
		if (!(readl(&regs->sr) & 0x01))
			return 0;
		udelay(1);
	}
	return -1;
}

static void set_tmod(RkSpiRegs *regs, unsigned int tmod)
{
	clrsetbits_le32(&regs->ctrlr0, SPI_TMOD_MASK << SPI_TMOD_OFFSET,
				      tmod << SPI_TMOD_OFFSET);
}

static void set_transfer_mode(RkSpiRegs *regs, void *in, const void *out)
{
	if (!in && !out)
		return;
	else if (in && out)
		set_tmod(regs, SPI_TMOD_TR);	// tx and rx
	else if (!in)
		set_tmod(regs, SPI_TMOD_TO);	// tx only
	else if (!out)
		set_tmod(regs, SPI_TMOD_RO);	// rx only
}

// returns 0 to indicate success, <0 otherwise
static int do_xfer(RkSpi *bus, void *in, const void *out, uint32_t size)
{
	RkSpiRegs *regs = bus->reg_addr;
	uint8_t *in_buf =in;
	uint8_t *out_buf = (uint8_t *)out;

	while (size) {
		uint32_t sr = readl(&regs->sr);
		int xferred = 0;	// in either (or both) directions

		if (out_buf && !(sr & SR_TF_FULL)) {
			writel(*out_buf, &regs->txdr);
			out_buf++;
			xferred = 1;
		}

		if (in_buf && !(sr & SR_RF_EMPT)) {
			*in_buf = readl(&regs->rxdr) & 0xff;
			in_buf++;
			xferred = 1;
		}

		size -= xferred;
	}

	if (rockchip_spi_wait_till_not_busy(bus)) {
		printf("Timed out waiting on SPI transfer\n");
		return -1;
	}

	return 0;
}

static int spi_transfer(SpiOps *me, void *in, const void *out, uint32_t size)
{
	int res = 0;
	RkSpi *bus = container_of(me, RkSpi, ops);
	RkSpiRegs *regs = bus->reg_addr;

	spi_info("spi:: transfer\n");
	assert((in != NULL) ^ (out != NULL));

	// RK3288 SPI controller can transfer up to 65536 data frames (bytes
	// in our case) continuously. Break apart large requests as necessary.
	//
	// FIXME: And by 65536, we really mean 65535. If 0xffff is written to
	// ctrlr1, all bytes that we see in rxdr end up being 0x00. 0xffff - 1
	// seems to work fine.
	while (size) {
		unsigned int dataframes = MIN(size, 0xffff);

		writel(CONTROLLER_DISABLE, &regs->enr);

		writel(dataframes - 1, &regs->ctrlr1);

		// Disable transmitter and receiver as needed to avoid
		// sending or reading spurious bits.
		set_transfer_mode(regs, in, out);

		writel(CONTROLLER_ENABLE, &regs->enr);

		res = do_xfer(bus, in, out, dataframes);
		if (res < 0)
			break;

		if (in)
			in += dataframes;

		if (out)
			out += dataframes;

		size -= dataframes;
	}

	writel(CONTROLLER_DISABLE, &regs->enr);
	return res < 0 ? res : 0;

}

static int spi_start(SpiOps *me)
{
	int res = 0;
	RkSpi *bus = container_of(me, RkSpi, ops);
	RkSpiRegs *regs = bus->reg_addr;

	spi_info("spi:: start\n");
	writel(1, &regs->ser);
	return res;
}

static int spi_stop(SpiOps *me)
{
	int res = 0;
	RkSpi *bus = container_of(me, RkSpi, ops);
	RkSpiRegs *regs = bus->reg_addr;

	spi_info("spi:: stop\n");
	writel(0, &regs->ser);
	return res;
}

RkSpi *new_rockchip_spi(uintptr_t reg_addr)
{
	RkSpi *bus = NULL;

	bus = xzalloc(sizeof(*bus));
	bus->reg_addr = (void *)reg_addr;
	bus->ops.start = &spi_start;
	bus->ops.stop = &spi_stop;
	bus->ops.transfer = &spi_transfer;
	return bus;
}

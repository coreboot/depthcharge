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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
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
	u32 spi_ctrlr0;
	u32 spi_ctrlr1;
	u32 spi_enr;
	u32 spi_ser;
	u32 spi_baudr;
	u32 spi_txftlr;
	u32 spi_rxftlr;
	u32 spi_txflr;
	u32 spi_rxflr;
	u32 spi_sr;
	u32 spi_ipr;
	u32 spi_imr;
	u32 spi_isr;
	u32 spi_risr;
	u32 spi_icr;
	u32 spi_dmacr;
	u32 spi_dmatdlr;
	u32 spi_damrdlr;
	u32 reserved0[(0x400 - 0x48) / 4];
	u32 spi_txdr[0x100];
	u32 spi_rxdr[0x100];
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

//Slave select register
#define MAX_SLAVE	2

static int rockchip_spi_wait_till_not_busy(RkSpi *bus)
{
	unsigned int delay = 1000000;
	RkSpiRegs *regs = bus->reg_addr;

	while (delay--) {
		if (!(readl(&regs->spi_sr) & 0x01))
			return 0;
		udelay(1);
	}
	return -1;
}

static int spi_recv(RkSpi *bus, void *in, uint32_t size)
{
	uint32_t bytes_remaining;
	RkSpiRegs *regs = bus->reg_addr;
	int len = size;
	uint8_t *p = in;

	writel(CONTROLLER_DISABLE, &regs->spi_enr);
	clrsetbits_le32(&regs->spi_ctrlr0, SPI_TMOD_MASK << SPI_TMOD_OFFSET,
					   RXONLY << SPI_TMOD_OFFSET);

	while (len) {
		writel(CONTROLLER_DISABLE, &regs->spi_enr);
		bytes_remaining = MIN(len, 0xffff);
		writel(bytes_remaining - 1, &regs->spi_ctrlr1);
		len -= bytes_remaining;
		writel(CONTROLLER_ENABLE, &regs->spi_enr);
		while (bytes_remaining) {
			if (readl(&regs->spi_rxflr) & 0x3f) {
				*p++ = readl(&regs->spi_rxdr) & 0xff;
				bytes_remaining--;
			}
		}

		if (rockchip_spi_wait_till_not_busy(bus)) {
			printf("spi wait busy err\n");
			return -1;
		}
	}
	return 0;
}

static int spi_send(RkSpi *bus, const void *out, uint32_t size)
{
	uint32_t bytes_remaining = size;
	RkSpiRegs *regs = bus->reg_addr;
	uint8_t *p = (uint8_t *) out;
	int len;

	len = size - 1;
	writel(CONTROLLER_DISABLE, &regs->spi_enr);

	clrsetbits_le32(&regs->spi_ctrlr0, SPI_TMOD_MASK << SPI_TMOD_OFFSET,
					   TXONLY << SPI_TMOD_OFFSET);
	writel(len, &regs->spi_ctrlr1);
	writel(CONTROLLER_ENABLE, &regs->spi_enr);

	while (bytes_remaining) {
		if ((readl(&regs->spi_txflr) & 0x3f) < FIFO_DEPTH) {
			writel(*p++, &regs->spi_txdr);
			bytes_remaining--;
		}
	}

	if (rockchip_spi_wait_till_not_busy(bus))
		return -1;
	return 0;
}

static int spi_transfer(SpiOps *me, void *in, const void *out, uint32_t size)
{
	int res = 0;
	RkSpi *bus = container_of(me, RkSpi, ops);

	spi_info("spi:: transfer\n");
	assert((in != NULL) ^ (out != NULL));

	if (out != NULL)
		res = spi_send(bus, out, size);
	if (in != NULL)
		res = spi_recv(bus, in, size);
	return res;
}

static int spi_start(SpiOps *me)
{
	int res = 0;
	unsigned int cr0 = 0;
	RkSpi *bus = container_of(me, RkSpi, ops);
	RkSpiRegs *regs = bus->reg_addr;

	spi_info("spi:: start\n");
	cr0 |= 1 << CR0_HALFWORD_TS_BIT;
	cr0 |= 1 << CR0_SSN_DELAY_BIT;
	cr0 |= (bus->polarity << CR0_CLOCK_POLARITY_BIT);
	cr0 |= (bus->phase << CR0_CLOCK_PHASE_BIT);
	cr0 |= FRAME_SIZE_8BIT << CR0_FRAME_SIZE_BIT;
	writel(cr0, &regs->spi_ctrlr0);

	writel(FIFO_DEPTH / 2 - 1, &regs->spi_txflr);
	writel(FIFO_DEPTH / 2 - 1, &regs->spi_rxflr);
	writel(1, &regs->spi_ser);
	return res;
}

static int spi_stop(SpiOps *me)
{
	int res = 0;
	RkSpi *bus = container_of(me, RkSpi, ops);
	RkSpiRegs *regs = bus->reg_addr;

	spi_info("spi:: stop\n");
	writel(0, &regs->spi_ser);
	return res;
}

RkSpi *new_rockchip_spi(uintptr_t reg_addr, unsigned int polarity,
			  unsigned int cs, unsigned int phase)
{
	RkSpi *bus = NULL;

	die_if(cs >= MAX_SLAVE, "spi cs exceed max slave\n");

	bus = xzalloc(sizeof(*bus));
	bus->reg_addr = (void *)reg_addr;
	bus->cs = cs;
	bus->polarity = polarity;
	bus->phase = phase;
	bus->ops.start = &spi_start;
	bus->ops.stop = &spi_stop;
	bus->ops.transfer = &spi_transfer;
	return bus;
}

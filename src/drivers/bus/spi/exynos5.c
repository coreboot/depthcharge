/*
 * Copyright (C) 2011 Samsung Electronics
 * Copyright 2013 Google Inc. All rights reserved.
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>
#include <stdint.h>

#include "base/container_of.h"
#include "drivers/bus/spi/exynos5.h"


// SPI peripheral register map.
typedef struct ExynosSpiRegs {
	uint32_t ch_cfg;	/* 0x00 */
	uint32_t reserved0;
	uint32_t mode_cfg;	/* 0x08 */
	uint32_t cs_reg;	/* 0x0c */
	uint32_t reserved1;
	uint32_t spi_sts;	/* 0x14 */
	uint32_t tx_data;	/* 0x18 */
	uint32_t rx_data;	/* 0x1c */
	uint32_t pkt_cnt;	/* 0x20 */
	uint32_t reserved2;
	uint32_t swap_cfg;	/* 0x28 */
	uint32_t fb_clk;	/* 0x2c */
} ExynosSpiRegs;


// SPI_CHCFG
enum {
	SPI_CH_HS_EN = (1 << 6),
	SPI_CH_RST = (1 << 5),
	SPI_SLAVE_MODE = (1 << 4),
	SPI_CH_CPOL_L = (1 << 3),
	SPI_CH_CPHA_B = (1 << 2),
	SPI_RX_CH_ON = (1 << 1),
	SPI_TX_CH_ON = (1 << 0)
};

// SPI_MODECFG
enum {
	SPI_MODE_CH_WIDTH_MASK = (0x3 << 29),
	SPI_MODE_CH_WIDTH_BYTE = (0x0 << 29),
	SPI_MODE_CH_WIDTH_WORD = (0x2 << 29),

	SPI_MODE_BUS_WIDTH_MASK = (0x3 << 17),
	SPI_MODE_BUS_WIDTH_BYTE = (0x0 << 17),
	SPI_MODE_BUS_WIDTH_WORD = (0x2 << 17)
};

// SPI_CSREG
enum {
	SPI_SLAVE_SIG_INACT = (1 << 0)
};

// SPI_STS
enum {
	SPI_ST_TX_DONE = (1 << 25),
	SPI_FIFO_LVL_MASK = 0x1ff,
	SPI_TX_LVL_OFFSET = 6,
	SPI_RX_LVL_OFFSET = 15
};

// Feedback Delay
enum {
	SPI_CLK_BYPASS = (0 << 0),
	SPI_FB_DELAY_90 = (1 << 0),
	SPI_FB_DELAY_180 = (2 << 0),
	SPI_FB_DELAY_270 = (3 << 0)
};

// Packet Count
enum {
	SPI_PACKET_CNT_EN = (1 << 16)
};

// Swap config
enum {
	SPI_TX_SWAP_EN = (1 << 0),
	SPI_TX_BYTE_SWAP = (1 << 2),
	SPI_TX_HWORD_SWAP = (1 << 3),
	SPI_RX_SWAP_EN = (1 << 4),
	SPI_RX_BYTE_SWAP = (1 << 6),
	SPI_RX_HWORD_SWAP = (1 << 7)
};


static void setbits32(uint32_t *data, uint32_t bits)
{
	writel(readl(data) | bits, data);
}

static void clrbits32(uint32_t *data, uint32_t bits)
{
	writel(readl(data) & ~bits, data);
}

static void exynos5_spi_sw_reset(ExynosSpiRegs *regs, int word)
{
	const uint32_t orig_mode_cfg = readl(&regs->mode_cfg);
	uint32_t mode_cfg = orig_mode_cfg;
	const uint32_t orig_swap_cfg = readl(&regs->swap_cfg);
	uint32_t swap_cfg = orig_swap_cfg;

	mode_cfg &= ~(SPI_MODE_CH_WIDTH_MASK | SPI_MODE_BUS_WIDTH_MASK);
	if (word) {
		mode_cfg |= SPI_MODE_CH_WIDTH_WORD | SPI_MODE_BUS_WIDTH_WORD;
		swap_cfg |= SPI_RX_SWAP_EN |
			    SPI_RX_BYTE_SWAP |
			    SPI_RX_HWORD_SWAP |
			    SPI_TX_SWAP_EN |
			    SPI_TX_BYTE_SWAP |
			    SPI_TX_HWORD_SWAP;
	} else {
		mode_cfg |= SPI_MODE_CH_WIDTH_BYTE | SPI_MODE_BUS_WIDTH_BYTE;
		swap_cfg = 0;
	}

	if (mode_cfg != orig_mode_cfg)
		writel(mode_cfg, &regs->mode_cfg);
	if (swap_cfg != orig_swap_cfg)
		writel(swap_cfg, &regs->swap_cfg);

	clrbits32(&regs->ch_cfg, SPI_RX_CH_ON | SPI_TX_CH_ON);
	setbits32(&regs->ch_cfg, SPI_CH_RST);
	clrbits32(&regs->ch_cfg, SPI_CH_RST);
	setbits32(&regs->ch_cfg, SPI_RX_CH_ON | SPI_TX_CH_ON);
}


// Set up SPI channel.
static void exynos5_spi_init(ExynosSpiRegs *regs)
{
	// Set FB_CLK_SEL.
	writel(SPI_FB_DELAY_180, &regs->fb_clk);
	// CPOL: Active high.
	clrbits32(&regs->ch_cfg, SPI_CH_CPOL_L);

	// Clear rx and tx channel if set priveously.
	clrbits32(&regs->ch_cfg, SPI_RX_CH_ON | SPI_TX_CH_ON);

	setbits32(&regs->swap_cfg,
		     SPI_RX_SWAP_EN | SPI_RX_BYTE_SWAP | SPI_RX_HWORD_SWAP);
	clrbits32(&regs->ch_cfg, SPI_CH_HS_EN);

	// Do a soft reset, which will also enable both channels.
	exynos5_spi_sw_reset(regs, 1);
}

static int exynos5_spi_start(SpiOps *me)
{
	Exynos5Spi *bus = container_of(me, Exynos5Spi, ops);
	ExynosSpiRegs *regs = bus->reg_addr;

	if (!bus->initialized) {
		exynos5_spi_init(regs);
		bus->initialized = 1;
	}

	if (bus->started) {
		printf("%s: Transaction already started.\n", __func__);
		return -1;
	}

	// Make CS low.
	clrbits32(&regs->cs_reg, SPI_SLAVE_SIG_INACT);
	bus->started = 1;

	return 0;
}

static int exynos5_spi_transfer(SpiOps *me, void *in, const void *out,
				uint32_t size)
{
	Exynos5Spi *bus = container_of(me, Exynos5Spi, ops);
	ExynosSpiRegs *regs = bus->reg_addr;

	if (!bus->started) {
		printf("%s: Transaction not started.\n", __func__);
		return -1;
	}

	uint8_t *inb = in;
	const uint8_t *outb = out;

	int width = (size % 4) ? 1 : 4;

	while (size) {
		int packets = size / width;
		// The packet count field is 16 bits wide.
		packets = MIN(packets, (1 << 16) - 1);

		int out_bytes, in_bytes;
		out_bytes = in_bytes = packets * width;

		exynos5_spi_sw_reset(regs, width == 4);
		writel(packets | SPI_PACKET_CNT_EN, &regs->pkt_cnt);

		while (out_bytes || in_bytes) {
			uint32_t spi_sts = readl(&regs->spi_sts);
			int rx_lvl = ((spi_sts >> 15) & 0x1ff);
			int tx_lvl = ((spi_sts >> 6) & 0x1ff);

			if (tx_lvl < 32 && tx_lvl < out_bytes) {
				uint32_t data = 0xffffffff;

				if (outb) {
					memcpy(&data, outb, width);
					outb += width;
				}
				writel(data, &regs->tx_data);

				out_bytes -= width;
			}

			if (rx_lvl >= width) {
				uint32_t data = readl(&regs->rx_data);

				if (inb) {
					memcpy(inb, &data, width);
					inb += width;
				}

				in_bytes -= width;
			}
		}

		size -= packets * width;
	}

	return 0;
}

static int exynos5_spi_stop(SpiOps *me)
{
	Exynos5Spi *bus = container_of(me, Exynos5Spi, ops);
	ExynosSpiRegs *regs = bus->reg_addr;

	if (!bus->started) {
		printf("%s: Transaction not yet started.\n", __func__);
		return -1;
	}

	// Make CS high.
	setbits32(&regs->cs_reg, SPI_SLAVE_SIG_INACT);
	bus->started = 0;

	return 0;
}

Exynos5Spi *new_exynos5_spi(uintptr_t reg_addr)
{
	Exynos5Spi *bus = xzalloc(sizeof(*bus));
	bus->ops.start = &exynos5_spi_start;
	bus->ops.transfer = &exynos5_spi_transfer;
	bus->ops.stop = &exynos5_spi_stop;
	bus->reg_addr = (void *)reg_addr;
	return bus;
}

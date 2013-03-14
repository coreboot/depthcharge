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

#include <assert.h>
#include <libpayload.h>
#include <stdint.h>

#include "config.h"
#include "drivers/flash/flash.h"


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

static ExynosSpiRegs * const regs =
	(ExynosSpiRegs *)(uintptr_t)CONFIG_DRIVER_FLASH_EXYNOS5_SPI_REGADDR;


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
	SPI_MODE_CH_WIDTH_WORD = (0x2 << 29),
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

static void exynos5_spi_sw_reset(void)
{
	clrbits32(&regs->ch_cfg, SPI_RX_CH_ON | SPI_TX_CH_ON);
	setbits32(&regs->ch_cfg, SPI_CH_RST);
	clrbits32(&regs->ch_cfg, SPI_CH_RST);
	setbits32(&regs->ch_cfg, SPI_RX_CH_ON | SPI_TX_CH_ON);
}


static void exynos5_spi_read(uint8_t *buf, uint32_t offset, int bytes)
{
	// Make CS low.
	clrbits32(&regs->cs_reg, SPI_SLAVE_SIG_INACT);

	const uint8_t ReadCommand = 0x3;

	int words = (bytes + sizeof(uint32_t) - 1) / sizeof(uint32_t);

	// Send the command and address.
	exynos5_spi_sw_reset();
	writel(1 | SPI_PACKET_CNT_EN, &regs->pkt_cnt);
	writel((ReadCommand << 24) | offset, &regs->tx_data);

	// Wait for it to be transmitted.
	while (!(readl(&regs->spi_sts) & SPI_ST_TX_DONE));

	// Empty the RX FIFO.
	readl(&regs->rx_data);

	// Prepare to receive the data.
	exynos5_spi_sw_reset();
	writel(words | SPI_PACKET_CNT_EN, &regs->pkt_cnt);

	while (bytes) {
		uint32_t spi_sts = readl(&regs->spi_sts);
		int rx_lvl = ((spi_sts >> 15) & 0x1ff);
		int tx_lvl = ((spi_sts >> 6) & 0x1ff);

		// Send garbage to keep the FIFO clocking if there's room and
		// the number of pending outgoing bytes isn't enough to get
		// everything.
		if (tx_lvl < 32 && tx_lvl < words * sizeof(uint32_t))
			writel(0xffffffff, &regs->tx_data);

		if (rx_lvl >= sizeof(uint32_t)) {
			int received = MIN(sizeof(uint32_t), bytes);
			uint32_t data = readl(&regs->rx_data);
			memcpy(buf, &data, received);
			bytes -= received;
			buf += received;
			words--;
		}
	}

	// Make CS high.
	setbits32(&regs->cs_reg, SPI_SLAVE_SIG_INACT);
}

// Set up SPI channel.
static void exynos5_spi_init(void)
{
	static int done = 0;
	if (done)
		return;

	// Set FB_CLK_SEL.
	writel(SPI_FB_DELAY_180, &regs->fb_clk);
	// Set CH_WIDTH and BUS_WIDTH as word.
	setbits32(&regs->mode_cfg,
		     SPI_MODE_CH_WIDTH_WORD | SPI_MODE_BUS_WIDTH_WORD);
	// CPOL: Active high.
	clrbits32(&regs->ch_cfg, SPI_CH_CPOL_L);

	// Clear rx and tx channel if set priveously.
	clrbits32(&regs->ch_cfg, SPI_RX_CH_ON | SPI_TX_CH_ON);

	setbits32(&regs->swap_cfg,
		     SPI_RX_SWAP_EN | SPI_RX_BYTE_SWAP | SPI_RX_HWORD_SWAP);
	clrbits32(&regs->ch_cfg, SPI_CH_HS_EN);

	// Do a soft reset, which will also enable both channels.
	exynos5_spi_sw_reset();

	done = 1;
}

static uint8_t exynos5_spi_cache[CONFIG_DRIVER_FLASH_EXYNOS5_SPI_ROM_SIZE]
	__attribute__((aligned(16)));

void *flash_read(uint32_t offset, uint32_t size)
{
	uint8_t *data = exynos5_spi_cache + offset;

	exynos5_spi_init();

	while (size) {
		// The packet count field is only 16 bits wide.
		int todo = MIN(size, ((1 << 16) - 1) * sizeof(uint32_t));

		exynos5_spi_read(exynos5_spi_cache + offset, offset, todo);
		offset += todo;
		size -= todo;
	}

	return data;
}

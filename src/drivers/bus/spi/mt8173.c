/*
 * Copyright 2015 MediaTek Inc.
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

#include <libpayload.h>
#include <stdint.h>
#include <stdlib.h>
#include <arch/virtual.h>

#include "base/container_of.h"
#include "drivers/bus/spi/mt8173.h"

enum {
	MTK_PACKET_SIZE = 1024,
	MTK_TXRX_TIMEOUT_US = 1000 * 1000
};

enum {
	MTK_SPI_IDLE = 0,
	MTK_SPI_PAUSE_IDLE = 1
};

enum {
	MTK_SPI_BUSY_STATUS = 1,
	MTK_SPI_PAUSE_FINISH_INT_STATUS = 3
};

static void mtk_spi_reset(MtkSpiRegs *regs)
{
	setbits_le32(&regs->spi_cmd_reg, 1 << SPI_CMD_RST_SHIFT);
	clrbits_le32(&regs->spi_cmd_reg, 1 << SPI_CMD_RST_SHIFT);
}

static void mtk_spi_dump_data(const char *name, const uint8_t *data, int size)
{
#ifdef MTK_SPI_DEBUG
	int i;

	printf("%s: 0x ", name);
	for (i = 0; i < size; i++)
		printf("%#x ", data[i]);
	printf("\n");
#endif
}

static int mtk_spi_start(SpiOps *me)
{
	MtkSpi *bus = container_of(me, MtkSpi, ops);
	MtkSpiRegs *regs = bus->reg_addr;

	bus->state = MTK_SPI_IDLE;

	/* set pause mode */
	setbits_le32(&regs->spi_cmd_reg, 1 << SPI_CMD_PAUSE_EN_SHIFT);

	return 0;
}

static int mtk_spi_transfer(SpiOps *me, void *in, const void *out, uint32_t size)
{
	MtkSpi *bus = container_of(me, MtkSpi, ops);
	MtkSpiRegs *regs = bus->reg_addr;
	uint32_t packet_len, packet_loop;
	uint64_t start;
	uint8_t *inb = NULL, *outb = NULL;

	/* The SPI controller will transmit in full-duplex for RX,
	 * therefore we enable tx & rx DMA when just do RX.
	 */
	if (!in && out)
		setbits_le32(&regs->spi_cmd_reg, 1 << SPI_CMD_TX_DMA_SHIFT);
	else
		setbits_le32(&regs->spi_cmd_reg, (1 << SPI_CMD_RX_DMA_SHIFT) |
			     (1 << SPI_CMD_TX_DMA_SHIFT));

	if (in)
		inb = dma_malloc(size);

	outb = dma_malloc(size);

	if (out)
		memcpy(outb, out, size);

	/* set transfer packet and loop */
	packet_len = MIN(size, MTK_PACKET_SIZE);
	packet_loop = div_round_up(size, packet_len);

	clrsetbits_le32(&regs->spi_cfg1_reg, SPI_CFG1_PACKET_LENGTH_MASK |
			SPI_CFG1_PACKET_LOOP_MASK,
			((packet_len - 1) << SPI_CFG1_PACKET_LENGTH_SHIFT) |
			((packet_loop - 1) << SPI_CFG1_PACKET_LOOP_SHIFT));


	if (out)
		mtk_spi_dump_data("the outb data is", (const uint8_t *)outb, size);

	write32(&regs->spi_tx_src_reg, (uintptr_t)outb);

	if (in)
		write32(&regs->spi_rx_dst_reg, (uintptr_t)inb);

	if (bus->state == MTK_SPI_IDLE) {
		setbits_le32(&regs->spi_cmd_reg, 1 << SPI_CMD_ACT_SHIFT);
		bus->state = MTK_SPI_PAUSE_IDLE;
	} else if (bus->state == MTK_SPI_PAUSE_IDLE) {
		setbits_le32(&regs->spi_cmd_reg, 1 << SPI_CMD_RESUME_SHIFT);
	}

	/* spi sw should wait for status1 register to idle before polling
	 * status0 register for rx/tx finish.
	 */
	start = timer_us(0);
	while ((read32(&regs->spi_status1_reg) & MTK_SPI_BUSY_STATUS) == 0) {
		if (timer_us(start) > MTK_TXRX_TIMEOUT_US) {
			printf("Timeout waiting for spi status1 register.\n");
			goto error;
		}
	}
	start = timer_us(0);
	while ((read32(&regs->spi_status0_reg) &
	       MTK_SPI_PAUSE_FINISH_INT_STATUS) == 0) {
		if (timer_us(start) > MTK_TXRX_TIMEOUT_US) {
			printf("Timeout waiting for spi status0 register.\n");
			goto error;
		}
	}

	if (in) {
		mtk_spi_dump_data("the inb data is", (const uint8_t *)inb, size);
		memcpy(in, inb, size);
	}

	clrbits_le32(&regs->spi_cmd_reg, (1 << SPI_CMD_RX_DMA_SHIFT) |
		     (1 << SPI_CMD_TX_DMA_SHIFT));

	free(inb);
	free(outb);

	return 0;
error:
	mtk_spi_reset(regs);
	bus->state = MTK_SPI_IDLE;
	clrbits_le32(&regs->spi_cmd_reg, (1 << SPI_CMD_RX_DMA_SHIFT) |
		     (1 << SPI_CMD_TX_DMA_SHIFT));
	free(inb);
	free(outb);
	return -1;

}

static int mtk_spi_stop(SpiOps *me)
{
	MtkSpi *bus = container_of(me, MtkSpi, ops);
	MtkSpiRegs *regs = bus->reg_addr;

	mtk_spi_reset(regs);
	bus->state = MTK_SPI_IDLE;

	/* clear pause mode */
	clrbits_le32(&regs->spi_cmd_reg, 1 << SPI_CMD_PAUSE_EN_SHIFT);

	return 0;
}

MtkSpi *new_mtk_spi(uintptr_t reg_addr)
{
	MtkSpi *bus = xzalloc(sizeof(*bus));

	bus->ops.start = &mtk_spi_start;
	bus->ops.transfer = &mtk_spi_transfer;
	bus->ops.stop = &mtk_spi_stop;
	bus->reg_addr = (MtkSpiRegs *)reg_addr;
	return bus;
}

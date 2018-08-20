/*
 * Copyright 2014 MediaTek Inc.
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

#ifndef __DRIVERS_BUS_SPI_MTK_H__
#define __DRIVERS_BUS_SPI_MTK_H__

#include "drivers/bus/spi/spi.h"

/* SPI peripheral register map. */
typedef struct {
	uint32_t spi_cfg0_reg;
	uint32_t spi_cfg1_reg;
	uint32_t spi_tx_src_reg;
	uint32_t spi_rx_dst_reg;
	uint32_t spi_tx_data_reg;
	uint32_t spi_rx_data_reg;
	uint32_t spi_cmd_reg;
	uint32_t spi_status0_reg;
	uint32_t spi_status1_reg;
	uint32_t spi_pad_macro_sel_reg;
} MtkSpiRegs;


/* SPI_CFG1_REG */
enum {
	SPI_CFG1_PACKET_LOOP_SHIFT = 8,
	SPI_CFG1_PACKET_LENGTH_SHIFT = 16,
	SPI_CFG1_PACKET_LOOP_MASK = 0xff << SPI_CFG1_PACKET_LOOP_SHIFT,
	SPI_CFG1_PACKET_LENGTH_MASK = 0x3ff << SPI_CFG1_PACKET_LENGTH_SHIFT,
};

enum {
	SPI_CMD_ACT_SHIFT = 0,
	SPI_CMD_RESUME_SHIFT = 1,
	SPI_CMD_RST_SHIFT = 2,
	SPI_CMD_PAUSE_EN_SHIFT = 4,
	SPI_CMD_RX_DMA_SHIFT = 10,
	SPI_CMD_TX_DMA_SHIFT = 11,
};

typedef struct {
	SpiOps ops;
	MtkSpiRegs *reg_addr;
	int state;
} MtkSpi;

MtkSpi *new_mtk_spi(uintptr_t reg_addr);
#endif /* __DRIVERS_BUS_SPI_MTK_H__ */

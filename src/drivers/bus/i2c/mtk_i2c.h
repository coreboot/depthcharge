/*
 * Copyright 2023 Mediatek Inc.
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
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_I2C_H__
#define __MTK_I2C_H__

#include "drivers/bus/i2c/i2c.h"
#include <libpayload.h>
#include <assert.h>
#include <stddef.h>

enum i2c_modes {
	I2C_WRITE_MODE		= 0,
	I2C_READ_MODE		= 1,
	I2C_WRITE_READ_MODE	= 2,
};

enum {
	I2C_DMA_CON_TX		= 0x0,
	I2C_DMA_CON_RX		= 0x1,
	I2C_DMA_START_EN	= 0x1,
	I2C_DMA_INT_FLAG_NONE	= 0x0,
	I2C_DMA_CLR_FLAG	= 0x0,
	I2C_DMA_FLUSH_FLAG	= 0x1,
	I2C_DMA_ASYNC_MODE	= 0x0004,
	I2C_DMA_SKIP_CONFIG	= 0x0010,
	I2C_DMA_DIR_CHANGE	= 0x0200,
	I2C_DMA_WARM_RST	= 0x1,
	I2C_DMA_HARD_RST	= 0x2,
	I2C_DMA_HANDSHAKE_RST	= 0x4,
};

/* I2C DMA Registers */
struct mtk_i2c_dma_regs {
	uint32_t dma_int_flag;
	uint32_t dma_int_en;
	uint32_t dma_en;
	uint32_t dma_rst;
	uint32_t reserved1;
	uint32_t dma_flush;
	uint32_t dma_con;
	uint32_t dma_tx_mem_addr;
	uint32_t dma_rx_mem_addr;
	uint32_t dma_tx_len;
	uint32_t dma_rx_len;
};

check_member(mtk_i2c_dma_regs, dma_tx_len, 0x24);

#if CONFIG(DRIVER_BUS_I2C_MT8173)
#include "drivers/bus/i2c/mt8173.h"
#elif CONFIG(DRIVER_BUS_I2C_MT8183)
#include "drivers/bus/i2c/mt8183.h"
#elif CONFIG(DRIVER_BUS_I2C_MT8186)
#include "drivers/bus/i2c/mt8186.h"
#elif CONFIG(DRIVER_BUS_I2C_MT8188)
#include "drivers/bus/i2c/mt8188.h"
#else
#error "Unsupported I2C for MediaTek"
#endif

typedef struct MTKI2c {
	I2cOps ops;
	struct mtk_i2c_regs *base;
	struct mtk_i2c_dma_regs *dma_base;
	uint8_t *write_buffer;
	uint8_t *read_buffer;
	uint32_t flag;
} MTKI2c;

enum {
	I2C_TRANS_LEN_MASK	= (0xff),
	I2C_TRANS_AUX_LEN_MASK	= (0x1f << 8),
	I2C_CONTROL_MASK	= (0x3f << 1)
};

enum {
	I2C_APDMA_NOASYNC	= 0,
	I2C_APDMA_ASYNC		= 1,
};

/* Register mask */
enum {
	I2C_HS_NACKERR		= (1 << 2),
	I2C_ACKERR		= (1 << 1),
	I2C_TRANSAC_COMP	= (1 << 0),
};

/* reset bits */
enum {
	I2C_CLR_FLAG		= 0x0,
	I2C_SOFT_RST		= 0x1,
	I2C_HANDSHAKE_RST	= 0x20,
};

/* i2c control bits */
enum {
	ACK_ERR_DET_EN		= (1 << 5),
	DIR_CHG			= (1 << 4),
	CLK_EXT			= (1 << 3),
	DMA_EN			= (1 << 2),
	REPEATED_START_FLAG	= (1 << 1),
	STOP_FLAG		= (0 << 1)
};

/* I2C Status Code */
enum {
	I2C_OK				= 0x0000,
	I2C_SET_SPEED_FAIL_OVER_SPEED	= 0xA001,
	I2C_TRANSFER_INVALID_LENGTH	= 0xA002,
	I2C_TRANSFER_FAIL_HS_NACKERR	= 0xA003,
	I2C_TRANSFER_FAIL_ACKERR	= 0xA004,
	I2C_TRANSFER_FAIL_TIMEOUT	= 0xA005,
	I2C_TRANSFER_INVALID_ARGUMENT	= 0xA006
};

/* -----------------------------------------------------------------------
 * mtk i2c init function.
 *   i2c:    I2C chip config.
 *   Returns: bus
 */
MTKI2c *new_mtk_i2c(uintptr_t base, uintptr_t dma_base, uint32_t flag);

#endif /* __MTK_I2C_H__ */

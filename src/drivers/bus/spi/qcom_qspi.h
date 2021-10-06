/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2018-2019, The Linux Foundation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef __DRIVERS_BUS_SPI_QCOM_QSPI_H__
#define __DRIVERS_BUS_SPI_QCOM_QSPI_H__

#include <stdbool.h>
#include "drivers/bus/spi/spi.h"

#if CONFIG(DRIVER_BUS_QSPI_SC7180)
#define QSPI_CS_GPIO GPIO(68)
#endif

typedef struct {
	u32 mstr_cfg;
	u32 ahb_mstr_cfg;
	u32 reserve_0;
	u32 mstr_int_en;
	u32 mstr_int_sts;
	u32 pio_xfer_ctrl;
	u32 pio_xfer_cfg;
	u32 pio_xfer_sts;
	u32 pio_dataout_1byte;
	u32 pio_dataout_4byte;
	u32 rd_fifo_cfg;
	u32 rd_fifo_sts;
	u32 rd_fifo_rst;
	u32 reserve_1[3];
	u32 next_dma_desc_addr;
	u32 current_dma_desc_addr;
	u32 current_mem_addr;
	u32 hw_version;
	u32 rd_fifo[16];
} QcomQspiRegs;

typedef struct {
	uint32_t data_address;
	uint32_t next_descriptor;
	uint32_t direction:1;
	uint32_t multi_io_mode:3;
	uint32_t reserved1:4;
	uint32_t fragment:1;
	uint32_t reserved2:7;
	uint32_t length:16;
	uint32_t bounce_src;
	uint32_t bounce_dst;
	uint32_t bounce_length;
	uint64_t padding[5];
} QcomQspiDescriptor;

typedef struct {
	SpiOps ops;
	QcomQspiRegs *qspi_base;
	QcomQspiDescriptor *first_descriptor;
	QcomQspiDescriptor *last_descriptor;
} QcomQspi;

QcomQspi *new_qcom_qspi(uintptr_t base);

#endif  /* __DRIVERS_BUS_SPI_QCOM_QSPI_H__ */

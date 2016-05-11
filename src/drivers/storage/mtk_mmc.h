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

#ifndef __DRIVERS_STORAGE_MTK_MMC_H_
#define __DRIVERS_STORAGE_MTK_MMC_H_

#include <arch/io.h>

#include "drivers/gpio/gpio.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/mmc.h"

typedef struct {
	uint32_t msdc_cfg;
	uint32_t msdc_iocon;
	uint32_t msdc_ps;
	uint32_t msdc_int;
	uint32_t msdc_inten;
	uint32_t msdc_fifocs;
	uint32_t msdc_txdata;
	uint32_t msdc_rxdata;
	uint8_t reserve0[0x10];
	uint32_t sdc_cfg;
	uint32_t sdc_cmd;
	uint32_t sdc_arg;
	uint32_t sdc_sts;
	uint32_t sdc_resp0;
	uint32_t sdc_resp1;
	uint32_t sdc_resp2;
	uint32_t sdc_resp3;
	uint32_t sdc_blk_num;
	uint32_t reserver1;
	uint32_t sdc_csts;
	uint32_t sdc_csts_en;
	uint32_t sdc_datcrc_sts;
	uint8_t reserve2[0x0c];
	uint32_t emmc_cfg0;
	uint32_t emmc_cfg1;
	uint32_t emmc_sts;
	uint32_t emmc_iocon;
	uint32_t sd_acmd_resp;
	uint32_t sd_acmd19_trg;
	uint32_t sd_acmd19_sts;
	uint32_t reserve3;
	uint32_t dma_sa;
	uint32_t dma_ca;
	uint32_t dma_ctrl;
	uint32_t dma_cfg;
	uint32_t sw_dbg_sel;
	uint32_t sw_dbg_out;
	uint32_t dma_length;
	uint32_t reserve4;
	uint32_t patch_bit0;
	uint32_t patch_bit1;
	uint8_t reserve5[0x34];
	uint32_t pad_tune;
	uint32_t dat_rd_dly0;
	uint32_t dat_rd_dly1;
	uint32_t hw_dbg_sel;
	uint32_t dummy10;
	uint32_t main_ver;
	uint32_t eco_ver;
} MtkMmcReg;

typedef struct {
	uint32_t msdc_iocon;
	uint32_t pad_tune;
} MtkMmcTuneReg;

typedef struct {
	MmcCtrlr mmc;

	MtkMmcReg *reg;
	MtkMmcTuneReg tune_reg;
	uint32_t clock;         /* Current clock (MHz) */
	uint32_t src_hz;        /* Source clock (hz) */

	GpioOps *cd_gpio;	/* Change Detect GPIO */

	int initialized;
	int removable;
} MtkMmcHost;

MtkMmcHost *new_mtk_mmc_host(uintptr_t ioaddr, uint32_t src_hz,
			     uint32_t max_freq, MtkMmcTuneReg tune_reg,
			     int bus_width, int removable, GpioOps *card_detect);
#endif // __DRIVERS_STORAGE_MTK_MMC_H_

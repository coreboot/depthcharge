/*
 * Copyright 2013 Google LLC
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

#ifndef __DRIVERS_FLASH_MTK_SNFC_H__
#define __DRIVERS_FLASH_MTK_SNFC_H__

#include <stdint.h>
#include "drivers/flash/flash.h"
#include "drivers/storage/blockdev.h"

/* SPI-NOR Flash Controller (SNFC) register */
typedef struct {
	uint32_t cmd;
	uint32_t cnt;
	uint32_t rdsr;
	uint32_t rdata;
	uint32_t radr[3];
	uint32_t wdata;
	uint32_t prgdata[6];
	uint32_t shreg[10];
	uint32_t cfg[2];
	uint32_t shreg10;
	uint32_t status[5];
	uint32_t timing;
	uint32_t flash_cfg;
	uint32_t reserved2[3];
	uint32_t sf_time;
	uint32_t reserved3;
	uint32_t diff_addr;
	uint32_t del_sel[2];
	uint32_t intrstus;
	uint32_t intren;
	uint32_t pp_ctl;
	uint32_t cfg3;
	uint32_t chksum_ctl;
	uint32_t chksum;
	uint32_t aaicmd;
	uint32_t wrprot;
	uint32_t radr3;
	uint32_t read_dual;
	uint32_t delsel[3];
} mtk_snfc_regs;

typedef struct {
	FlashOps ops;
	mtk_snfc_regs *reg;
	uint32_t rom_size;
	uint8_t *buffer;
} MtkNorFlash;

MtkNorFlash *new_mtk_nor_flash(uintptr_t reg_addr);

#endif

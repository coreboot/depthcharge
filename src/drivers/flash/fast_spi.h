/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2018 Google LLC
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

#ifndef __DRIVERS_FLASH_FAST_SPI_H__
#define __DRIVERS_FLASH_FAST_SPI_H__

#include <stdint.h>

#include "drivers/flash/flash.h"

typedef enum {
	FLASH_REGION_DESCRIPTOR = 0,
	FLASH_REGION_BIOS,
	FLASH_REGION_ME,
	FLASH_REGION_GBE,
	FLASH_REGION_PDR,
	FLASH_REGION_EC = 8,
	FLASH_REGION_MAX
} FlashRegionId;

typedef struct FlashRegion {
	uint32_t offset;
	uint32_t size;
} FlashRegion;

typedef struct FastSpiFlash {
	uintptr_t mmio_base;
	uint32_t flash_bits;
	uint32_t rom_size;
	uint8_t *cache;
	FlashRegion region[FLASH_REGION_MAX];
	FlashOps ops;
} FastSpiFlash;

FastSpiFlash *new_fast_spi_flash(uintptr_t mmio_base, uint32_t rom_size);

#endif /* __DRIVERS_FLASH_FAST_SPI_H__ */

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

#ifndef __DRIVERS_FLASH_SPI_H__
#define __DRIVERS_FLASH_SPI_H__

#include <stdint.h>

#include "drivers/flash/flash.h"

struct SpiOps;
typedef struct SpiOps SpiOps;

typedef struct
{
	FlashOps ops;
	SpiOps *spi;
	uint32_t rom_size;
	uint8_t erase_cmd;
	uint8_t *cache;
} SpiFlash;

SpiFlash *new_spi_flash(SpiOps *spi);

#endif /* __DRIVERS_FLASH_FLASH_H__ */

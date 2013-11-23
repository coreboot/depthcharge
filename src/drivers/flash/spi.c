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

#include <endian.h>
#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/bus/spi/spi.h"
#include "drivers/flash/spi.h"

const uint8_t ReadCommand = 0x3;

static void *spi_flash_read(FlashOps *me, uint32_t offset, uint32_t size)
{
	SpiFlash *flash = container_of(me, SpiFlash, ops);
	uint8_t *data = flash->cache + offset;

	if (flash->spi->start(flash->spi)) {
		printf("%s: Failed to start flash transaction.\n", __func__);
		return NULL;
	}

	uint32_t command = swap_bytes32((ReadCommand << 24) | offset);

	if (flash->spi->transfer(flash->spi, NULL, &command, sizeof(command))) {
		printf("%s: Failed to send read command.\n", __func__);
		flash->spi->stop(flash->spi);
		return NULL;
	}

	if (flash->spi->transfer(flash->spi, data, NULL, size)) {
		printf("%s: Failed to receive %u bytes.\n", __func__, size);
		flash->spi->stop(flash->spi);
		return NULL;
	}

	if (flash->spi->stop(flash->spi)) {
		printf("%s: Failed to stop transaction.\n", __func__);
		return NULL;
	}

	return data;
}

SpiFlash *new_spi_flash(SpiOps *spi, uint32_t rom_size)
{
	SpiFlash *flash = xmalloc(sizeof(*flash) + rom_size);
	memset(flash, 0, sizeof(*flash));
	flash->ops.read = &spi_flash_read;
	flash->spi = spi;
	flash->rom_size = rom_size;
	return flash;
}

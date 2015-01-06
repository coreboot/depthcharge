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

typedef enum {
	ReadCommand = 3,
	ReadSr1Command = 5,
	WriteCommand = 2,
	WriteEnableCommand = 6
} SpiFlashCommands;

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

/* Generate a 10 us 'CS inactive' pulse. */
static int toggle_cs(SpiFlash *flash, const char *phase)
{
	if (flash->spi->stop(flash->spi)) {
		printf("%s: Failed to stop after %s.\n", __func__, phase);
		return -1;
	}

	udelay(10); /* 10 us is plenty of time. */

	if (flash->spi->start(flash->spi)) {
		printf("%s: Failed to start after %s.\n", __func__, phase);
		return -1;
	}

	return 0;
}

/* Allow up to 100 ms for a transaction to complete */
#define POLL_INTERVAL_US 10
#define MAX_POLL_CYCLES (100000/POLL_INTERVAL_US)
#define SPI_FLASH_STATUS_WIP (1 << 0)
#define SPI_FLASH_STATUS_ERASE_ERR (1 << 5)
#define SPI_FLASH_STATUS_PROG_ERR (1 << 6)
#define SPI_FLASH_STATUS_ERR (SPI_FLASH_STATUS_ERASE_ERR | \
			      SPI_FLASH_STATUS_PROG_ERR)

/*
 * Poll device status register until write/erase operation has completed or
 * timed out. Returns zero on success.
 */
static int operation_failed(SpiFlash *flash)
{
	uint8_t value;
	int i = 0;

	if (toggle_cs(flash, "write"))
		return -1;

	value = ReadSr1Command;
	if (flash->spi->transfer(flash->spi, NULL, &value, sizeof(value))) {
		printf("%s: Failed to send register read command.\n",
		       __func__);
		return -1;
	}

	while (i++ < MAX_POLL_CYCLES) {
		if (flash->spi->transfer(flash->spi, &value,
					 NULL, sizeof(value))) {
			printf("%s: Failed to read status after %d cycles.\n",
			       __func__, i);
			return -1;
		}
		if (!(value & SPI_FLASH_STATUS_WIP)) {
			if (value & SPI_FLASH_STATUS_ERR) {
				printf("%s: status %#x after %d cycles.\n",
				       __func__, value, i);
				return -1;
			}
			return 0;
		}
	}
	printf("%s: timeout waiting for write completion\n", __func__);
	return -1;
}

/*
 * This function is guaranteed to be invoked with data not spanning across
 * writeable page boundaries.
 */
static int spi_flash_write_restricted(SpiFlash *flash, const void *buffer,
				      uint32_t offset, uint32_t size)
{
	union {
		uint8_t bytes[4]; // We're using 3 byte addresses.
		uint32_t whole;
	} command;

	int stop_needed = 0;
	uint32_t rv = -1;

	do {
		/* Each write command requires a 'write enable' (WREN) first. */
		if (flash->spi->start(flash->spi)) {
			printf("%s: Failed to start WREN transaction.\n",
			       __func__);
			break;
		}

		command.bytes[0] = WriteEnableCommand;
		if (flash->spi->transfer(flash->spi, NULL, &command, 1)) {
			printf("%s: Failed to send write enable command.\n",
			       __func__);
			stop_needed = 1;
			break;
		}

		/*
		 * CS needs to be deasserted before any other command can be
		 * issued after WREN.
		 */
		if (toggle_cs(flash, "WREN"))
			break;

		stop_needed = 1;
		command.whole = swap_bytes32((WriteCommand << 24) | offset);
		if (flash->spi->transfer(flash->spi, NULL, &command, 4)) {
			printf("%s: Failed to send write command.\n", __func__);
			break;
		}

		if (flash->spi->transfer(flash->spi, NULL, buffer, size)) {
			printf("%s: Failed to write data.\n", __func__);
			break;
		}

		stop_needed = 1;
		if (!operation_failed(flash))
			rv = size;

	} while(0);

	if (stop_needed && flash->spi->stop(flash->spi))
		printf("%s: Failed to stop.\n", __func__);

	return rv;
}

#define SPI_FLASH_WRITE_PAGE_LIMIT (1 << 8)
#define SPI_WRITE_PAGE_MASK (~(SPI_FLASH_WRITE_PAGE_LIMIT - 1))

static int spi_flash_write(FlashOps *me, const void *buffer,
			   uint32_t offset, uint32_t size)
{
	SpiFlash *flash = container_of(me, SpiFlash, ops);
	uint32_t written = 0;

	/* Write in chunks guaranteed not to cross page boundaries, */
	while (size) {
		uint32_t write_size, page_offset;

		page_offset = offset & ~SPI_WRITE_PAGE_MASK;
		write_size = (page_offset + size) > SPI_FLASH_WRITE_PAGE_LIMIT ?
			SPI_FLASH_WRITE_PAGE_LIMIT - page_offset : size;

		if (spi_flash_write_restricted(flash, buffer,
					       offset, write_size) !=
		    write_size)
			break;

		offset += write_size;
		size -= write_size;
		buffer = (char *)buffer + write_size;
		written += write_size;
	}

	return written;
}

SpiFlash *new_spi_flash(SpiOps *spi, uint32_t rom_size)
{
	SpiFlash *flash = xmalloc(sizeof(*flash) + rom_size);
	memset(flash, 0, sizeof(*flash));
	flash->ops.read = spi_flash_read;
	flash->ops.write = spi_flash_write;
	flash->spi = spi;
	flash->rom_size = rom_size;
	return flash;
}

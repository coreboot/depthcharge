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

#include <assert.h>
#include <endian.h>
#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/bus/spi/spi.h"
#include "drivers/flash/spi.h"

typedef enum {
	ReadCommand = 3,
	ReadSr1Command = 5,
	WriteStatus = 1,
	WriteCommand = 2,
	WriteEnableCommand = 6,
	ReadId = 0x9f
} SpiFlashCommands;

static void *spi_flash_read(FlashOps *me, uint32_t offset, uint32_t size)
{
	SpiFlash *flash = container_of(me, SpiFlash, ops);
	uint8_t *data = flash->cache + offset;

	assert(offset + size <= flash->rom_size);

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

/* Allow up to 2s for a transaction to complete
 * This value is used for both write and erase */
#define POLL_INTERVAL_US 10
#define MAX_POLL_CYCLES (2000000/POLL_INTERVAL_US)
#define SPI_FLASH_STATUS_WIP (1 << 0)
#define SPANSION_FLASH_ERASE_ERR (1 << 5)
#define SPANSION_FLASH_PROG_ERR (1 << 6)
#define SPANSION_FLASH_ERR (SPANSION_FLASH_PROG_ERR | \
				SPANSION_FLASH_ERASE_ERR)

#define SPANSION_ID		0x01

/*
 * Check if a programming/erase error occurred.
 */
static int check_error(SpiFlash *flash, uint8_t status)
{
	uint8_t id[6];	/* worst-case scenario for Spansion */
	uint8_t cmd = ReadId;

	if (flash->spi->transfer(flash->spi, NULL, &cmd, sizeof(cmd))) {
		printf("%s: Failed to send register read command.\n",
		       __func__);
		return -1;
	}

	if (flash->spi->transfer(flash->spi, &id, NULL, sizeof(id))) {
		printf("%s: Failed to send register read command.\n",
		       __func__);
		return -1;
	}

	switch(id[0]) {
	case SPANSION_ID:
		/* For now, assume all Spansion chips we care about act
		   the same with regards to error checking. */
		if (status & SPANSION_FLASH_ERR)
			return -1;
		break;
	default:
		break;
	}

	return 0;
}

/*
 * Poll device status register until write/erase operation has completed or
 * timed out. Returns zero on success.
 */
static int operation_failed(SpiFlash *flash, const char *opname)
{
	uint8_t value;
	int i = 0;

	if (toggle_cs(flash, opname))
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
			if (check_error(flash, value)) {
				printf("%s: status %#x after %d cycles.\n",
				       __func__, value, i);
				return -1;
			}
			return 0;
		}

		udelay(POLL_INTERVAL_US);
	}
	printf("%s: timeout waiting for %s completion\n", __func__, opname);
	return -1;
}

/*
 * This checks if the status register protect bit is set. Write-protection
 * is active when both of the following conditions are met:
 * 1. Status register protect bit is set to 1.
 * 2. Hardware write-protect pin (/WP) is asserted.
 */
static int spi_flash_is_wp_enabled(struct FlashOps *ops)
{
	int value = ops->read_status(ops);

	/*
	 * NOR flash chips we currently support write-protection for all have
	 * SRP0 in bit position 7. If this changes, we'll need to read the chip
	 * ID and take appropriate action (like with error checking).
	 */
	return value & (1 << 7) ? 1 : 0;
}

/*
 * Write or erase the flash. To write, pass a buffer and size; to erase,
 * pass null for the buffer.
 * This function is guaranteed to be invoked with data not spanning across
 * writeable/erasable boundaries (page size/block size).
 */
static int spi_flash_modify(SpiFlash *flash, const void *buffer,
			    uint32_t offset, uint32_t size, uint8_t opcode,
			    const char *opname)
{
	union {
		uint8_t bytes[4]; // We're using 3 byte addresses.
		uint32_t whole;
	} command;

	int stop_needed = 0;
	uint32_t rv = -1;

	do {
		/* Each write or erase command requires a 'write enable' (WREN)
		 * first. */
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
		command.whole = swap_bytes32((opcode << 24) | offset);
		if (flash->spi->transfer(flash->spi, NULL, &command, 4)) {
			printf("%s: Failed to send %s command.\n",
			       __func__, opname);
			break;
		}

		if (buffer &&
		    flash->spi->transfer(flash->spi, NULL, buffer, size)) {
			printf("%s: Failed to write data.\n", __func__);
			break;
		}

		stop_needed = 1;
		if (!operation_failed(flash, opname))
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

	assert(offset + size <= flash->rom_size);

	/* Write in chunks guaranteed not to cross page boundaries, */
	while (size) {
		uint32_t write_size, page_offset;

		page_offset = offset & ~SPI_WRITE_PAGE_MASK;
		write_size = (page_offset + size) > SPI_FLASH_WRITE_PAGE_LIMIT ?
			SPI_FLASH_WRITE_PAGE_LIMIT - page_offset : size;

		if (spi_flash_modify(flash, buffer, offset, write_size,
				     WriteCommand, "write") !=
		    write_size)
			break;

		offset += write_size;
		size -= write_size;
		buffer = (char *)buffer + write_size;
		written += write_size;
	}

	return written;
}

static int spi_flash_erase(FlashOps *me, uint32_t start, uint32_t size)
{
	SpiFlash *flash = container_of(me, SpiFlash, ops);
	uint32_t sector_size = me->sector_size;

	if ((start % sector_size) || (size % sector_size)) {
		printf("%s: Erase not %u aligned, start=%u size=%u\n",
		       __func__, sector_size, start, size);
		return -1;
	}
	assert(start + size <= flash->rom_size);
	int offset;
	for (offset = 0; offset < size; offset += sector_size) {
		if (spi_flash_modify(flash, NULL, start + offset, 0,
				     flash->erase_cmd, "erase"))
			break;
	}
	return offset;
}

static int spi_flash_read_status(FlashOps *me)
{
	int ret = -1;
	SpiFlash *flash = container_of(me, SpiFlash, ops);

	uint8_t command;

	if (flash->spi->start(flash->spi)) {
		printf("%s: Failed to start transaction.\n", __func__);
		return ret;
	}

	if (toggle_cs(flash, "RDSTATUS") != 0)
		goto fail;

	command = ReadSr1Command;
	if (flash->spi->transfer(flash->spi, NULL, &command, sizeof(command))) {
		printf("%s: Failed to send register read command.\n", __func__);
		goto fail;
	}

	if (flash->spi->transfer(flash->spi, &command, NULL, sizeof(command))) {
		printf("%s: Failed to read status.\n", __func__);
		goto fail;
	}

	ret = command;
fail:
	if (flash->spi->stop(flash->spi)) {
		printf("%s: Failed to stop.\n", __func__);
		ret = -1;
	}
	return ret;
}

static int spi_flash_write_status(FlashOps *me, uint8_t status)
{
	int ret = -1;
	SpiFlash *flash = container_of(me, SpiFlash, ops);

	uint8_t command_bytes[2];

	if (flash->spi->start(flash->spi)) {
		printf("%s: Failed to start transaction.\n", __func__);
		return ret;
	}

	command_bytes[0] = WriteEnableCommand;
	if (flash->spi->transfer(flash->spi, NULL, &command_bytes, 1)) {
		printf("%s: Failed to send write enable command.\n",
		       __func__);
		goto fail;
	}

	if (operation_failed(flash, "WREN") != 0)
		goto fail;

	/*
	 * CS needs to be deasserted before any other command can be
	 * issued after WREN.
	 */
	if (toggle_cs(flash, "WREN"))
		goto fail;

	command_bytes[0] = WriteStatus;
	command_bytes[1] = status;

	if (flash->spi->transfer(flash->spi, NULL, &command_bytes, 2)) {
		printf("%s: Failed to send write status command.\n", __func__);
		goto fail;
	}

	if (operation_failed(flash, "WRSTATUS") == 0)
		ret = 0;

fail:
	if (flash->spi->stop(flash->spi)) {
		printf("%s: Failed to stop.\n", __func__);
		ret = -1;
	}
	return ret;
}

SpiFlash *new_spi_flash(SpiOps *spi)
{
	uint32_t rom_size = lib_sysinfo.spi_flash.size;
	uint32_t sector_size = lib_sysinfo.spi_flash.sector_size;
	uint8_t erase_cmd = lib_sysinfo.spi_flash.erase_cmd;

	SpiFlash *flash = xmalloc(sizeof(*flash));
	memset(flash, 0, sizeof(*flash));
	flash->ops.read = spi_flash_read;
	flash->ops.write = spi_flash_write;
	flash->ops.erase = spi_flash_erase;
	flash->ops.write_status = spi_flash_write_status;
	flash->ops.read_status = spi_flash_read_status;
	flash->ops.is_wp_enabled = spi_flash_is_wp_enabled;
	flash->ops.sector_size = sector_size;
	assert(rom_size == ALIGN_DOWN(rom_size, sector_size));
	flash->ops.sector_count = rom_size / sector_size;
	flash->spi = spi;
	flash->rom_size = rom_size;
	flash->erase_cmd = erase_cmd;
	/* Provide sufficient alignment on the cache buffer so that the
	 * underlying SPI controllers can perform optimal DMA transfers. */
	flash->cache = xmemalign(1*KiB, rom_size);
	return flash;
}

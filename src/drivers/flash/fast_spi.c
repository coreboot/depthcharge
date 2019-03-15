// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2017 Intel Corporation.
 * Copyright 2018 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>
#include <stdint.h>
#include <assert.h>

#include "base/container_of.h"
#include "drivers/flash/fast_spi.h"
#include "drivers/flash/fast_spi_def.h"
#include "drivers/flash/flash.h"

/* Read register from the FAST_SPI flash controller. */
static uint32_t fast_spi_flash_ctrlr_reg_read(FastSpiFlash *flash, uint16_t reg)
{
	uintptr_t addr =  ALIGN_DOWN(flash->mmio_base + reg, sizeof(uint32_t));
	return read32((void *)addr);
}

/* Write to register in FAST_SPI flash controller. */
static void fast_spi_flash_ctrlr_reg_write(FastSpiFlash *flash,
					   uint16_t reg, uint32_t val)
{
	uintptr_t addr =  ALIGN_DOWN(flash->mmio_base + reg, sizeof(uint32_t));
	write32((void *)addr, val);
}

/* Fill FDATAn FIFO in preparation for a write transaction. */
static void fill_xfer_fifo(FastSpiFlash *flash, const void *data, size_t size)
{
	memcpy((void *)(flash->mmio_base + SPIBAR_FDATA(0)), data, size);
}

/* Drain FDATAn FIFO after a read transaction populates data. */
static void drain_xfer_fifo(FastSpiFlash *flash, void *dest, size_t size)
{
	memcpy(dest, (void *)(flash->mmio_base + SPIBAR_FDATA(0)), size);
}

/* Fire up a transfer using the hardware sequencer. */
static void start_hwseq_xfer(FastSpiFlash *flash, uint32_t hsfsts_cycle,
			     uint32_t offset, size_t size)
{
	/* Make sure all W1C status bits get cleared. */
	uint32_t hsfsts = SPIBAR_HSFSTS_W1C_BITS;

	/* Set up transaction parameters. */
	hsfsts |= hsfsts_cycle & SPIBAR_HSFSTS_FCYCLE_MASK;
	hsfsts |= SPIBAR_HSFSTS_FDBC(size - 1);

	fast_spi_flash_ctrlr_reg_write(flash, SPIBAR_FADDR, offset);
	fast_spi_flash_ctrlr_reg_write(flash, SPIBAR_HSFSTS_CTL,
				       hsfsts | SPIBAR_HSFSTS_FGO);
}

static int wait_for_hwseq_xfer(FastSpiFlash *flash, uint32_t offset)
{
	uint64_t start = timer_us(0);
	uint32_t hsfsts;

	do {
		hsfsts = fast_spi_flash_ctrlr_reg_read(flash,
						       SPIBAR_HSFSTS_CTL);

		if (hsfsts & SPIBAR_HSFSTS_FCERR) {
			printf("SPI Transaction Error @ %x HSFSTS = 0x%08x\n",
			       offset, hsfsts);
			return -1;
		}

		if (hsfsts & SPIBAR_HSFSTS_FDONE)
			return 0;
	} while (timer_us(start) < SPIBAR_XFER_TIMEOUT_US);

	printf("SPI Transaction Timeout @ %x HSFSTS = 0x%08x\n",
	       offset, hsfsts);
	return -1;
}

/* Execute FAST_SPI flash transfer. This is a blocking call. */
static int exec_sync_hwseq_xfer(FastSpiFlash *flash, uint32_t hsfsts_cycle,
				uint32_t offset, size_t size)
{
	start_hwseq_xfer(flash, hsfsts_cycle, offset, size);
	return wait_for_hwseq_xfer(flash, offset);
}

/*
 * Ensure read/write xfer len is not greater than SPIBAR_FDATA_FIFO_SIZE and
 * that the operation does not cross page boundary.
 */
static size_t get_xfer_len(uint32_t offset, size_t size)
{
	size_t xfer_len = MIN(size, SPIBAR_FDATA_FIFO_SIZE);
	size_t bytes_left = ALIGN_UP(offset, SPIBAR_PAGE_SIZE) - offset;

	if (bytes_left)
		xfer_len = MIN(xfer_len, bytes_left);

	return xfer_len;
}

static int fast_spi_flash_erase(FlashOps *me, uint32_t offset, uint32_t size)
{
	FastSpiFlash *flash = container_of(me, FastSpiFlash, ops);
	size_t erase_size;
	uint32_t erase_cycle, remaining;

	assert(offset + size <= flash->rom_size);

	if (!IS_ALIGNED(offset, 4 * KiB) || !IS_ALIGNED(size, 4 * KiB)) {
		printf("SPI erase region not sector aligned\n");
		return -1;
	}

	remaining = size;
	while (remaining) {
		if (IS_ALIGNED(offset, 64 * KiB) && (remaining >= 64 * KiB)) {
			erase_size = 64 * KiB;
			erase_cycle = SPIBAR_HSFSTS_CYCLE_64K_ERASE;
		} else {
			erase_size = 4 * KiB;
			erase_cycle = SPIBAR_HSFSTS_CYCLE_4K_ERASE;
		}
		printf("Erasing flash offset %x + %zu KiB\n",
		       offset, erase_size / KiB);

		if (exec_sync_hwseq_xfer(flash, erase_cycle, offset, 0) < 0)
			return -1;

		offset += erase_size;
		remaining -= erase_size;
	}
	return size;
}

static void *fast_spi_flash_read(FlashOps *me, uint32_t offset, uint32_t size)
{
	FastSpiFlash *flash = container_of(me, FastSpiFlash, ops);
	uint8_t *data = flash->cache + offset;
	uint8_t *odata = data;

	assert(offset + size <= flash->rom_size);

	/*
	 * If the read is entirely within the memory map just return a pointer
	 * within the memory mapped region
	 */
	if (offset >= flash->mmio_offset && offset + size < flash->mmio_end)
		return (void *)(uintptr_t)(flash->mmio_address -
					   flash->mmio_offset + offset);

	while (size) {
		size_t xfer_len = get_xfer_len(offset, size);

		if (exec_sync_hwseq_xfer(flash, SPIBAR_HSFSTS_CYCLE_READ,
					 offset, xfer_len) < 0)
			return NULL;

		drain_xfer_fifo(flash, data, xfer_len);

		offset += xfer_len;
		data += xfer_len;
		size -= xfer_len;
	}

	return odata;
}

static int fast_spi_flash_write(FlashOps *me, const void *buffer,
				uint32_t offset, uint32_t size)
{
	FastSpiFlash *flash = container_of(me, FastSpiFlash, ops);
	const uint8_t *data = buffer;
	uint32_t remaining;

	assert(offset + size <= flash->rom_size);

	remaining = size;
	while (remaining) {
		size_t xfer_len = get_xfer_len(offset, remaining);

		fill_xfer_fifo(flash, data, xfer_len);

		if (exec_sync_hwseq_xfer(flash, SPIBAR_HSFSTS_CYCLE_WRITE,
					 offset, xfer_len) < 0)
			return -1;

		offset += xfer_len;
		data += xfer_len;
		remaining -= xfer_len;
	}
	return size;
}

static JedecFlashId fast_spi_flash_read_id(FlashOps *me)
{
	JedecFlashId id = { 0 };
	FastSpiFlash *flash = container_of(me, FastSpiFlash, ops);
	uint8_t buffer[3];

	if (exec_sync_hwseq_xfer(flash, SPIBAR_HSFSTS_CYCLE_RD_ID,
				 0, sizeof(buffer)) < 0)
		return id;
	drain_xfer_fifo(flash, buffer, 3);

	id.vendor = buffer[0];
	id.model = (buffer[1] << 8) | buffer[2];

	return id;
}

static int fast_spi_flash_read_status(FlashOps *me)
{
	FastSpiFlash *flash = container_of(me, FastSpiFlash, ops);
	uint8_t status;

	if (exec_sync_hwseq_xfer(flash, SPIBAR_HSFSTS_CYCLE_RD_STATUS, 0, 1)
		< 0)
		return -1;
	drain_xfer_fifo(flash, &status, 1);

	return status;
}

static int fast_spi_flash_write_status(FlashOps *me, uint8_t status)
{
	FastSpiFlash *flash = container_of(me, FastSpiFlash, ops);

	fill_xfer_fifo(flash, &status, 1);
	if (exec_sync_hwseq_xfer(flash, SPIBAR_HSFSTS_CYCLE_WR_STATUS, 0, 1)
		< 0)
		return -1;

	return 0;
}

static void fast_spi_fill_regions(FastSpiFlash *flash)
{
	FlashRegion *region = flash->region;
	int i;

	/* Go through each flash region */
	for (i = 0; i < FLASH_REGION_MAX; i++, region++) {
		uintptr_t reg = flash->mmio_base + SPIBAR_FREG(i);
		uint32_t freg = read32((void *)reg);

		/* Extract region offset and size from FREG */
		region->offset = (freg & SPIBAR_FREG_BASE_MASK) * 4 * KiB;
		region->size = (((freg & SPIBAR_FREG_LIMIT_MASK) >>
				 SPIBAR_FREG_LIMIT_SHIFT) + 1) * 4 * KiB;

		/* Find invalid regions */
		if (region->size > region->offset)
			region->size -= region->offset;
		if (region->offset == 0x07fff000 && region->size == 0x1000)
			region->offset = region->size = 0;
	}
}

FastSpiFlash *new_fast_spi_flash(uintptr_t mmio_base)
{
	uint32_t rom_size = lib_sysinfo.spi_flash.size;
	uint32_t sector_size = lib_sysinfo.spi_flash.sector_size;

	uint32_t val = read32((void *)(mmio_base + SPIBAR_BIOS_BFPREG));

	uintptr_t mmap_start;
	size_t bios_base, bios_end, mmap_size;

	bios_base = (val & BFPREG_BASE_MASK) * 4 * KiB;
	bios_end  = (((val & BFPREG_LIMIT_MASK) >> BFPREG_LIMIT_SHIFT) + 1) *
		4 * KiB;
	mmap_size = bios_end - bios_base;

	/* Only the top 16 MiB is memory mapped */
	if (mmap_size > 16ULL * MiB) {
		uint32_t offset = mmap_size - 16ULL * MiB;
		bios_base += offset;
		mmap_size -= offset;
	}

	/* BIOS region is mapped directly below 4GiB. */
	mmap_start = 4ULL * GiB - mmap_size;

	printf("BIOS MMAP details:\n");
	printf("IFD Base Offset  : 0x%zx\n", bios_base);
	printf("IFD End Offset   : 0x%zx\n", bios_end);
	printf("MMAP Size        : 0x%zx\n", mmap_size);
	printf("MMAP Start       : 0x%lx\n", mmap_start);

	FastSpiFlash *flash = xzalloc(sizeof(*flash));

	flash->mmio_base = mmio_base;
	flash->rom_size = rom_size;
	flash->mmio_address = mmap_start;
	flash->mmio_offset = bios_base;
	flash->mmio_end = bios_end;
	flash->cache = xmalloc(flash->rom_size);

	flash->ops.sector_size = sector_size;
	flash->ops.read = fast_spi_flash_read;
	flash->ops.write = fast_spi_flash_write;
	flash->ops.erase = fast_spi_flash_erase;
	flash->ops.read_status = fast_spi_flash_read_status;
	flash->ops.write_status = fast_spi_flash_write_status;
	flash->ops.read_id = fast_spi_flash_read_id;

	fast_spi_fill_regions(flash);

	return flash;
}

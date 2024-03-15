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

#include <libpayload.h>

#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"

static const struct flash_mmap_window *find_mmap_window(uint32_t offset,
							uint32_t size)
{
	int i;
	const struct flash_mmap_window *window;

	window = lib_sysinfo.spi_flash.mmap_table;

	for (i = 0; i < lib_sysinfo.spi_flash.mmap_window_count; i++) {
		if ((offset >= window->flash_base) &&
		    ((offset + size) <= (window->flash_base + window->size)))
			return window;
		window++;
	}

	return NULL;
}

static int mmap_flash_read(void *buffer, uint32_t offset, uint32_t size,
			   const struct flash_mmap_window *window)
{
	/* Convert offset within flash space into an offset within host space */
	uint32_t rel_offset = offset - window->flash_base;
	memcpy(buffer, (void *)(uintptr_t)(window->host_base + rel_offset),
	       size);
	return size;
}

static int mmap_backed_flash_read(FlashOps *me, void *buffer, uint32_t offset,
				  uint32_t size)
{
	MmapFlash *flash = container_of(me, MmapFlash, ops);
	const struct flash_mmap_window *window = find_mmap_window(offset, size);

	if (window)
		return mmap_flash_read(buffer, offset, size, window);

	if (!flash->base_ops) {
		printf("ERROR: Offset(%#x)/size(%#x) out of bounds!\n",
		       offset, size);
		return -1;
	}

	return flash_read_ops(flash->base_ops, buffer, offset, size);
}

static int mmap_backed_flash_write(FlashOps *me, const void *buffer,
				   uint32_t offset, uint32_t size)
{
	MmapFlash *flash = container_of(me, MmapFlash, ops);
	return flash_write_ops(flash->base_ops, buffer, offset, size);
}

static int mmap_backed_flash_erase(FlashOps *me, uint32_t offset, uint32_t size)
{
	MmapFlash *flash = container_of(me, MmapFlash, ops);
	return flash_erase_ops(flash->base_ops, offset, size);
}

MmapFlash *new_mmap_backed_flash(FlashOps *base_ops)
{
	die_if(!lib_sysinfo.spi_flash.mmap_window_count,
	       "No MMAP windows for SPI flash!\n");

	MmapFlash *flash = xzalloc(sizeof(*flash));
	flash->ops.read = mmap_backed_flash_read;
	if (base_ops) {
		flash->ops.write = mmap_backed_flash_write;
		flash->ops.erase = mmap_backed_flash_erase;
		flash->base_ops = base_ops;
	}
	return flash;
}

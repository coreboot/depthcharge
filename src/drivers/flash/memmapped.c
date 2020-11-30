/*
 * Copyright 2013 Google Inc.
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

#include "base/container_of.h"
#include "base/init_funcs.h"
#include "drivers/flash/flash.h"

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

static void *mem_mapped_flash_read(FlashOps *me, uint32_t offset, uint32_t size)
{
	uint32_t rel_offset;
	const struct flash_mmap_window *window = find_mmap_window(offset, size);

	if (!window) {
		printf("ERROR: Offset(0x%x)/size(0x%x) out of bounds!\n",
		       offset, size);
		return NULL;
	}

	/* Convert offset within flash space into an offset within host space */
	rel_offset = offset - window->flash_base;
	return (void *)(uintptr_t)(window->host_base + rel_offset);
}

static int flash_setup(void)
{
	FlashOps *flash_ops;

	die_if(!lib_sysinfo.spi_flash.mmap_window_count,
	       "No MMAP windows for SPI flash!\n");

	flash_ops = xzalloc(sizeof(*flash_ops));

	flash_ops->read = &mem_mapped_flash_read;
	flash_set_ops(flash_ops);

	return 0;
}

INIT_FUNC(flash_setup);

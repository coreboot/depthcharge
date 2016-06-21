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

#ifndef __DRIVERS_FLASH_MEMMAPPED_H__
#define __DRIVERS_FLASH_MEMMAPPED_H__

#include <stdint.h>

#include "drivers/flash/flash.h"

typedef struct MemMappedFlash
{
	FlashOps ops;
	uint32_t base;
	uint32_t size;
	/* Offset of mmaped base within the flash. */
	uint32_t base_offset;
} MemMappedFlash;

MemMappedFlash *new_mem_mapped_flash(uint32_t base, uint32_t size);
/*
 * new_mem_mapped_flash_with_offset accepts an additional parameter base_offset
 * that identifies the offset in the flash which maps at the provided base
 * address. In most of the cases base_offset would be 0, indicating that flash
 * region starting from offset 0 is mapped onto the base address. However, in
 * some special cases (e.g. reef), flash region from offset 0x1000 is actually
 * mapped. Thus, base address corresponds to the offset 0x1000 on flash and
 * hence all addresses generated for mmap access on flash need to be adjusted
 * according to the base_offset.
 */
MemMappedFlash *new_mem_mapped_flash_with_offset(uint32_t base, uint32_t size,
						 uint32_t base_offset);

#endif /* __DRIVERS_FLASH_MEMMAPPED_H__ */

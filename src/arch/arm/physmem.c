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

#include <assert.h>
#include <libpayload.h>
#include <stddef.h>

#include <arch/cache.h>
#include "base/physmem.h"

// arch_phys_memset but with the guarantee that the range doesn't wrap around
// the end of the address space.
static void arch_phys_map_nowrap(uint64_t start, uint64_t size,
				 PhysMapFunc func, void *data)
{
	uint64_t max_addr = (uint64_t) ~(uintptr_t)0;

	// If there's nothing to do, just return.
	if (!size)
		return;

	// Set memory we can address normally using standard memset.
	if (start <= max_addr) {
		size_t low_size;
		// Be careful with this check to avoid overflow.
		if (size - 1 > max_addr - start) {
			low_size = 0 - start;
		} else {
			low_size = (size_t)size;
			assert(low_size == size);
		}
		func(start, (void *)(uintptr_t)start, low_size, data);
		start += low_size;
		size -= low_size;
	}

	// If we're done, return.
	if (!size)
		return;

	// memset above 4GB.
	do {
		void *buf;
		int len = MIN(size, 2 * MiB);
		/* writeback is ~4 times as fast as writethrough on T124 */
		buf = lpae_map_phys_addr(start / MiB, DCACHE_WRITEBACK);
		func(start, buf, len, data);
		start += len;
		size -= len;
	} while (size > 0);

	lpae_restore_map();
	return;
}

void arch_phys_map(uint64_t start, uint64_t size, PhysMapFunc func, void *data)
{
	uint64_t end = start + size;

	if (end && end < start) {
		// If the range wraps around 0, set the upper and lower parts
		// separately.
		arch_phys_map_nowrap(0, end, func, data);
		arch_phys_map_nowrap(start, 0 - start, func, data);
	} else {
		// No wrap, set everything at once.
		arch_phys_map_nowrap(start, size, func, data);
	}
}

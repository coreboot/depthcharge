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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <assert.h>
#include <libpayload.h>
#include <stddef.h>
#include <string.h>

#include <arch/cache.h>
#include "base/physmem.h"

// arch_phys_memset but with the guarantee that the range doesn't wrap around
// the end of the address space.
static uint64_t arch_phys_memset_nowrap(uint64_t start, int c, uint64_t size)
{
	uint64_t max_addr = (uint64_t)~(uintptr_t)0;
	uint64_t orig_start = start;

	// If there's nothing to do, just return.
	if (!size)
		return orig_start;

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
		memset((void *)(uintptr_t)start, c, low_size);
		start += low_size;
		size -= low_size;
	}

	// If we're done, return.
	if (!size)
		return orig_start;

	// memset above 4GB.
	do {
		void *buf;
		int len = MIN(size, 2*MiB);
		/* writeback is ~4 times as fast as writethrough on T124 */
		buf = lpae_map_phys_addr(start / MiB, DCACHE_WRITEBACK);
		memset(buf, c, len);
		start += len;
		size -= len;
	} while (size > 0);

	lpae_restore_map();
	return orig_start;
}

uint64_t arch_phys_memset(uint64_t start, int c, uint64_t size)
{
	uint64_t end = start + size;

	if (end && end < start) {
		// If the range wraps around 0, set the upper and lower parts
		// separately.
		arch_phys_memset_nowrap(0, c, end);
		return arch_phys_memset_nowrap(start, c, 0 - start);
	} else {
		// No wrap, set everything at once.
		return arch_phys_memset_nowrap(start, c, size);
	}
}

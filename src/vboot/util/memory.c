/*
 * Copyright (c) 2011-2012 The Chromium OS Authors.
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

#include <coreboot_tables.h>
#include <libpayload.h>
#include <stdint.h>
#include <sysinfo.h>

#include "base/ranges.h"
#include "base/physmem.h"
#include "image/symbols.h"
#include "vboot/util/memory.h"

void wipe_unused_memset(uint64_t start, uint64_t end, void *data)
{
	printf("\t[%#016llx, %#016llx)\n", start, end);
	arch_phys_memset(start, 0, end - start);
}

int wipe_unused_memory(void)
{
	Ranges ranges;

	// Process the memory map from coreboot.
	ranges_init(&ranges);
	for (int i = 0; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *range = &lib_sysinfo.memrange[i];
		phys_addr_t start = range->base;
		phys_addr_t end = range->base + range->size;
		switch (range->type) {
		case CB_MEM_RAM:
			ranges_add(&ranges, start, end);
			break;
		case CB_MEM_RESERVED:
		case CB_MEM_ACPI:
		case CB_MEM_NVS:
		case CB_MEM_UNUSABLE:
		case CB_MEM_VENDOR_RSVD:
		case CB_MEM_TABLE:
			ranges_sub(&ranges, start, end);
			break;
		default:
			printf("Unrecognized memory type %d!\n",
				range->type);
			return 1;
		}
	}

	// Exclude memory we're using ourselves.
	ranges_sub(&ranges, (uintptr_t)&_start, (uintptr_t)&_end);
	ranges_sub(&ranges, (uintptr_t)&_tramp_start, (uintptr_t)&_tramp_end);

	// Do the wipe.
	printf("Wipe memory regions:\n");
	ranges_for_each(&ranges, &wipe_unused_memset, NULL);
	ranges_teardown(&ranges);
	return 0;
}

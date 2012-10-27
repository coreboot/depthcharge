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

#include "image/symbols.h"
#include "vboot/util/memory.h"
#include "vboot/util/memory_wipe.h"
#include "vboot/util/physmem.h"

int wipe_unused_memory(void)
{
	memory_wipe_t wipe;

	// Process the memory map from coreboot.
	memory_wipe_init(&wipe);
	for (int i = 0; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *range = &lib_sysinfo.memrange[i];
		phys_addr_t start = range->base;
		phys_addr_t end = range->base + range->size;
		switch (range->type) {
		case CB_MEM_RAM:
			memory_wipe_add(&wipe, start, end);
			break;
		case CB_MEM_RESERVED:
		case CB_MEM_ACPI:
		case CB_MEM_NVS:
		case CB_MEM_UNUSABLE:
		case CB_MEM_VENDOR_RSVD:
		case CB_MEM_TABLE:
			memory_wipe_sub(&wipe, start, end);
			break;
		default:
			printf("Unrecognized memory type %d!\n",
				range->type);
			return 1;
		}
	}

	// Exclude memory we're using ourselves.
	memory_wipe_sub(&wipe, (uintptr_t)&_start, (uintptr_t)&_end);
	memory_wipe_sub(&wipe, (uintptr_t)&_tramp_start,
			(uintptr_t)&_tramp_end);

	// Do the wipe.
	memory_wipe_execute(&wipe);
	return 0;
}

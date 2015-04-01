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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
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

#include "arch/arm/boot.h"
#include "base/cleanup_funcs.h"
#include "base/timestamp.h"
#include "config.h"
#include "vboot/boot.h"
#include "base/ranges.h"
#include "base/physmem.h"
#include <stdlib.h>

void boot_arm_linux_jump(void *entry, void *fdt)
	__attribute__((noreturn));

static uintptr_t get_kernel_reloc_addr(uint64_t total_size)
{
	int i = 0;

	for (; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *range = &lib_sysinfo.memrange[i];

		if (range->type == CB_MEM_RAM) {
			uint64_t start = range->base;
			uint64_t end = range->base + range->size;
			uint64_t base = ALIGN_UP(start, 2*MiB);

			if (base + total_size < end) {
				return base;
			}
		}
	}
	return 0;
}

#define KERNEL_HEADER_MAGIC  0x644d5241
#define KERNEL_TEXT_OFFSET   1
#define KERNEL_MAGIC_OFFSET  7

int boot_arm_linux(uint32_t machine_type, void *fdt, void *entry,
		   uint32_t kernel_size)
{
	uint64_t *kernel_header = entry;
	uintptr_t new_base;
	void *reloc_addr;

	run_cleanup_funcs(CleanupOnHandoff);

	timestamp_add_now(TS_START_KERNEL);

	if (*(uint32_t*)(kernel_header + KERNEL_MAGIC_OFFSET) !=
	    KERNEL_HEADER_MAGIC) {
		printf("ERROR: Kernel Magic Header Fail!\n");
		return 1;
	}

	printf("Kernel Magic Header Match!\n");

	new_base = get_kernel_reloc_addr(kernel_size +
					 kernel_header[KERNEL_TEXT_OFFSET]);

	if (new_base == 0) {
		printf("ERROR: Cannot relocate kernel\n");
		return 1;
	}

	/* relocate kernel */
	reloc_addr = (void*)(new_base +
			     kernel_header[KERNEL_TEXT_OFFSET]);

	printf("Relocating kernel to %p\n", reloc_addr);
	memmove(reloc_addr, entry, kernel_size);
	entry = reloc_addr;

	printf("jumping to kernel\n");

	/* Flush dcache and icache to make loaded code visible. */
	arch_program_segment_loaded(reloc_addr, kernel_size);

	tlb_invalidate_all();

	boot_arm_linux_jump(entry, fdt);

	return 0;
}

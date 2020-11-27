/*
 * Copyright 2012 Google Inc.
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

#include <coreboot_tables.h>
#include <libpayload.h>
#include <stdint.h>
#include <sysinfo.h>

#if CONFIG(LP_ARCH_ARM64)
#include <arm64/arch/mmu.h>
#endif

#include "base/ranges.h"
#include "base/physmem.h"
#include "image/symbols.h"
#include "vboot/util/memory.h"

static Ranges used;

static void used_list_initialize(void)
{
	static int initialized;
	if (initialized)
		return;

	ranges_init(&used);

	// Add regions depthcharge occupies.
	ranges_add(&used, (uintptr_t)&_start, (uintptr_t)&_end);

	initialized = 1;
}

void memory_mark_used(uint64_t start, uint64_t end)
{
	used_list_initialize();
	ranges_add(&used, start, end);
}

static void arch_phys_memset_map_func(uint64_t phys_addr, void *s, uint64_t n,
				      void *data)
{
	memset(s, *((int *)data), n);
}

static inline uint64_t arch_phys_memset(uint64_t s, int c, uint64_t n)
{
	arch_phys_map(s, n, arch_phys_memset_map_func, &c);
	return s;
}

static inline void unused_memset(uint64_t start, uint64_t end, void *data)
{
	printf("\t[%#016llx, %#016llx)\n", start, end);
	arch_phys_memset(start, 0, end - start);
}

static void remove_range(uint64_t start, uint64_t end, void *data)
{
	ranges_sub((Ranges *)data, start, end);
}

static int get_unused_memory(Ranges *ranges)
{
	// Process the memory map from coreboot.
	for (int i = 0; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *range = &lib_sysinfo.memrange[i];
		uint64_t start = range->base;
		uint64_t end = range->base + range->size;
		switch (range->type) {
		case CB_MEM_RAM:
			ranges_add(ranges, start, end);
			break;
		case CB_MEM_RESERVED:
		case CB_MEM_ACPI:
		case CB_MEM_NVS:
		case CB_MEM_UNUSABLE:
		case CB_MEM_VENDOR_RSVD:
		case CB_MEM_TABLE:
			ranges_sub(ranges, start, end);
			break;
		default:
			printf("Unrecognized memory type %d!\n", range->type);
			return 1;
		}
	}

	// Exclude memory that's being used.
	used_list_initialize();
	ranges_for_each(&used, &remove_range, ranges);

	return 0;
}

static int get_mmu_used_memory(Ranges *ranges)
{
#if CONFIG(LP_ARCH_ARM64)
	// In ARM64, we may allocate DMA and framebuffer in libpayload
	// during MMU initialization.
	const struct mmu_ranges *usedmem_ranges = mmu_get_used_ranges();
	for (int i = 0; i < usedmem_ranges->used; i++) {
		const struct mmu_memrange *entry =
			&usedmem_ranges->entries[i];
		uint64_t start = entry->base;
		uint64_t end = entry->base + entry->size;
		switch (entry->type) {
		case TYPE_NORMAL_MEM:
		case TYPE_DEV_MEM:
		case TYPE_DMA_MEM:
			ranges_add(ranges, start, end);
			break;
		default:
			printf("Unrecognized memory type %lld!\n",
			       entry->type);
			return 1;
		}
	}
#endif
	return 0;
}

int memory_range_init_and_get_unused(Ranges *ranges)
{
	ranges_init(ranges);

	int result = get_unused_memory(ranges);
	if (result)
		return result;

	// Exclude the memory used in payload for MMU initialization.
	Ranges payload_used;
	ranges_init(&payload_used);
	result = get_mmu_used_memory(&payload_used);
	if (result) {
		ranges_teardown(&payload_used);
		return result;
	}
	ranges_for_each(&payload_used, &remove_range, ranges);
	ranges_teardown(&payload_used);

	return 0;
}

int memory_wipe_unused(void)
{
	// Do not exclude the memory used in payload. We would like to wipe
	// DMA and framebuffer for ARM64.
	Ranges ranges;
	ranges_init(&ranges);
	int result = get_unused_memory(&ranges);
	if (result) {
		ranges_teardown(&ranges);
		return result;
	}

	// Do the wipe.
	printf("Wipe memory regions:\n");
	ranges_for_each(&ranges, &unused_memset, NULL);
	ranges_teardown(&ranges);
	return result;
}

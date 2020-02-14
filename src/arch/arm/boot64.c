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
 */

#include <arch/mmu.h>
#include <coreboot_tables.h>
#include <libpayload.h>
#include <stdlib.h>

#include "arch/arm/boot.h"
#include "base/timestamp.h"
#include "vboot/boot.h"
#include "base/ranges.h"
#include "base/physmem.h"

typedef struct {
	u32 code0;
	u32 code1;
	u64 text_offset;
	u64 image_size;
	u64 flags;
#define KERNEL_FLAGS_PLACE_ANYWHERE	(1 << 3)
	u64 res2;
	u64 res3;
	u64 res4;
	u32 magic;
#define KERNEL_HEADER_MAGIC  0x644d5241
	u32 res5;
} Arm64KernelHeader;

static void *get_kernel_reloc_addr(Arm64KernelHeader *header, size_t image_size)
{
	size_t load_offset = header->text_offset;
	int i = 0;

	for (; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *range = &lib_sysinfo.memrange[i];
		if (range->type != CB_MEM_RAM)
			continue;

		uint64_t start = range->base;
		uint64_t end = range->base + range->size;
		uint64_t kstart = ALIGN_DOWN(start, 2*MiB) + load_offset < start
				? ALIGN_UP(start, 2*MiB) + load_offset
				: ALIGN_DOWN(start, 2*MiB) + load_offset;
		uint64_t kend = kstart + image_size;

		if (kend > CONFIG_BASE_ADDRESS || kend > CONFIG_KERNEL_START ||
		    kend > CONFIG_KERNEL_FIT_FDT_ADDR) {
			printf("ERROR: Kernel might overlap depthcharge!\n");
			return 0;
		}

		if (kend <= end)
			return (void *)kstart;

		// Should be avoided in practice, that memory might be wasted.
		if (!(header->flags & KERNEL_FLAGS_PLACE_ANYWHERE))
			printf("WARNING: Skipping low memory range [%p:%p]!\n",
			       (void *)start, (void *)end);
	}

	printf("ERROR: Cannot find enough continuous memory for kernel!\n");
	return 0;
}

int boot_arm_linux(void *fdt, FitImageNode *kernel)
{
	size_t image_size = 64*MiB;	// default value for pre-3.17 headers
	struct {
		union {
			Arm64KernelHeader header;
			u8 raw[sizeof(Arm64KernelHeader) + 0x100];
		};
	#define SCRATCH_CANARY_VALUE 0xdeadbeef
		u32 canary;
	} scratch;

	// Partially decompress to get text_offset. Can't check for errors.
	scratch.canary = SCRATCH_CANARY_VALUE;
	fit_decompress(kernel, scratch.raw, sizeof(scratch.raw));

	// Should never happen, but if it does we'll want to know.
	if (scratch.canary != SCRATCH_CANARY_VALUE) {
		printf("ERROR: Partial decompression ran over scratchbuf!\n");
		return 1;
	}

	if (scratch.header.magic != KERNEL_HEADER_MAGIC) {
		printf("ERROR: Invalid kernel magic: %#.8x\n != %#.8x\n",
		       scratch.header.magic, KERNEL_HEADER_MAGIC);
		return 1;
	}

	if (scratch.header.image_size)
		image_size = scratch.header.image_size;
	else
		printf("WARNING: Kernel image_size is 0 (pre-3.17 kernel?)\n");

	void *reloc_addr = get_kernel_reloc_addr(&scratch.header, image_size);
	if (!reloc_addr)
		return 1;

	timestamp_add_now(TS_KERNEL_DECOMPRESSION);

	size_t true_size = fit_decompress(kernel, reloc_addr, image_size);
	if (!true_size) {
		printf("ERROR: Kernel decompression failed!\n");
		return 1;
	}

	printf("jumping to kernel\n");

	timestamp_add_now(TS_START_KERNEL);

	/* Flush dcache and icache to make loaded code visible. */
	arch_program_segment_loaded(reloc_addr, true_size);

	tlb_invalidate_all();
	mmu_disable();

	void (*handoff)(void *, uintptr_t, uintptr_t, uintptr_t) = reloc_addr;
	handoff(fdt, 0, 0, 0);
	die("Kernel returned!");
}

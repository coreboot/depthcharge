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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <inttypes.h>
#include <libpayload.h>
#include <stdlib.h>

#include "arch/arm/boot.h"
#include "base/device_tree.h"
#include "base/timestamp.h"
#include "boot/dt_update.h"
#include "vboot/boot.h"
#include "vboot/stages.h"
#include "base/ranges.h"
#include "base/physmem.h"
#include "vboot/util/memory.h"

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

#define KERNEL_ALIGNMENT (2 * MiB)

struct kaslr_context {
	size_t image_size;
	size_t load_offset;
	uint32_t total_slots;
	uint32_t selected_slot;
	uintptr_t result_addr;
};

static uint64_t get_kstart(uint64_t start, uint64_t load_offset)
{
	uint64_t kstart = ALIGN_DOWN(start, KERNEL_ALIGNMENT) + load_offset;

	if (kstart < start)
		kstart += KERNEL_ALIGNMENT;

	return kstart;
}

static uint32_t get_kaslr_range(uint64_t start, uint64_t end, size_t image_size,
				size_t load_offset, uint64_t *kstart_out,
				uint64_t *kstart_last_out)
{
	uint64_t kstart = get_kstart(start, load_offset);

	if (kstart + image_size > end)
		return 0;

	uint64_t base_max = ALIGN_DOWN(end - image_size - load_offset,
				       KERNEL_ALIGNMENT);
	uint64_t kstart_last = base_max + load_offset;

	*kstart_out = kstart;
	*kstart_last_out = kstart_last;

	return (kstart_last - kstart) / KERNEL_ALIGNMENT + 1;
}

static void count_kaslr_slots(uint64_t start, uint64_t end, void *data)
{
	struct kaslr_context *ctx = data;
	uint64_t kstart, kstart_last;

	ctx->total_slots += get_kaslr_range(start, end, ctx->image_size, ctx->load_offset,
					    &kstart, &kstart_last);
}

static void pick_kaslr_slot(uint64_t start, uint64_t end, void *data)
{
	struct kaslr_context *ctx = data;
	uint64_t kstart, kstart_last;

	if (ctx->result_addr)
		return;

	uint32_t slots_in_range = get_kaslr_range(start, end, ctx->image_size, ctx->load_offset,
						  &kstart, &kstart_last);

	if (ctx->selected_slot < slots_in_range)
		ctx->result_addr = kstart + (uint64_t)ctx->selected_slot * KERNEL_ALIGNMENT;
	else
		ctx->selected_slot -= slots_in_range;
}

static void *get_kernel_reloc_random_addr(struct boot_info *bi,
					  Arm64KernelHeader *header,
					  size_t image_size)
{
	Ranges available;

	/* 1. Collect all available RAM regions from coreboot/MMU */
	if (memory_range_init_and_get_unused(&available))
		return 0;

	/* 2. Subtract DT reserved memory */
	Ranges reserved;
	dt_collect_reserved_memory_ranges(bi->dt, &reserved);
	ranges_sub_from(&available, &reserved);
	ranges_teardown(&reserved);

	/* 3. Subtract the [Depthcharge _start, 4GB) 'danger zone' per b/504966477#comment13 */
	uint64_t min_start = MIN(CONFIG_BASE_ADDRESS,
				 MIN(CONFIG_KERNEL_START, CONFIG_KERNEL_FIT_FDT_ADDR));
	assert(min_start < 4ULL * GiB);
	ranges_sub(&available, min_start, 4ULL * GiB);

	/* 4. Prepare KASLR context */
	struct kaslr_context ctx = {
		.image_size = image_size,
		.load_offset = header->text_offset,
		.total_slots = 0,
		.selected_slot = 0,
		.result_addr = 0,
	};

	/* 5. Count total available slots */
	ranges_for_each(&available, count_kaslr_slots, &ctx);

	if (ctx.total_slots == 0) {
		printf("ERROR: Cannot find enough continuous memory for kernel!\n");
		ranges_teardown(&available);
		return 0;
	}

	/* 6. Pick a random slot */
	if (CONFIG(ARM64_SKIP_PHYS_KASLR))
		ctx.selected_slot = 0;
	else
		ctx.selected_slot = bi->phys_kaslr % ctx.total_slots;

	ranges_for_each(&available, pick_kaslr_slot, &ctx);

	ranges_teardown(&available);

	return (void *)ctx.result_addr;
}

int boot_arm_linux(struct boot_info *bi, void *fdt, FitImageNode *kernel)
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
		printf("ERROR: Invalid kernel magic: %#.8x != %#.8x\n",
		       scratch.header.magic, KERNEL_HEADER_MAGIC);
		return 1;
	}

	if (scratch.header.image_size)
		image_size = scratch.header.image_size;
	else
		printf("WARNING: Kernel image_size is 0 (pre-3.17 kernel?)\n");

	void *reloc_addr = get_kernel_reloc_random_addr(bi, &scratch.header, image_size);

	if (!reloc_addr)
		return 1;

	timestamp_add_now(TS_KERNEL_DECOMPRESSION);

	size_t true_size = fit_decompress(kernel, reloc_addr, image_size);
	if (!true_size) {
		printf("ERROR: Kernel decompression failed!\n");
		return 1;
	}

	if (CONFIG(BOOTCONFIG))
		append_android_bootconfig_boottime(bi);

	printf("jumping to kernel\n");

	timestamp_add_now(TS_START_KERNEL);

	/* Flush dcache and icache to make loaded code visible. */
	arch_program_segment_loaded(reloc_addr, true_size);

	if (CONFIG(WIDEVINE_PROVISION)) {
		void *dma_start;
		size_t dma_size;
		dma_allocator_range(&dma_start, &dma_size);
		memset(dma_start, 0, dma_size);
	}

	boot_arm64_linux_jump(fdt, reloc_addr);

	die("Kernel returned!");
}

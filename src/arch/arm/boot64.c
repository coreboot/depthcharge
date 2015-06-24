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
#include <lzma.h>
#include <lz4.h>
#include <stdlib.h>

#include "arch/arm/boot.h"
#include "base/timestamp.h"
#include "config.h"
#include "vboot/boot.h"
#include "base/ranges.h"
#include "base/physmem.h"

#define MAX_KERNEL_SIZE (64*MiB)

typedef struct {
	u32 code0;
	u32 code1;
	u64 text_offset;
	u64 image_size;
	u64 flags;
	u64 res2;
	u64 res3;
	u64 res4;
	u32 magic;
#define KERNEL_HEADER_MAGIC  0x644d5241
	u32 res5;
} Arm64KernelHeader;

static void *get_kernel_reloc_addr(uint32_t load_offset)
{
	int i = 0;

	for (; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *range = &lib_sysinfo.memrange[i];
		if (range->type != CB_MEM_RAM)
			continue;

		uint64_t start = range->base;
		uint64_t end = range->base + range->size;
		uint64_t kstart = ALIGN_UP(start, 2*MiB) + load_offset;
		uint64_t kend = kstart + MAX_KERNEL_SIZE;

		if (kend > CONFIG_BASE_ADDRESS || kend > CONFIG_KERNEL_START ||
		    kend > CONFIG_KERNEL_FIT_FDT_ADDR) {
			printf("ERROR: Kernel might overlap depthcharge!\n");
			return 0;
		}

		if (kend <= end)
			return (void *)kstart;

		// Should be avoided in practice, that memory might be wasted.
		printf("WARNING: Skipping low memory range [%p:%p]!\n",
			       (void *)start, (void *)end);
	}

	printf("ERROR: Cannot find enough continuous memory for kernel!\n");
	return 0;
}

int boot_arm_linux(void *fdt, FitImageNode *kernel)
{
	// Duplicating text_offset in the FIT is our custom hack to simplify
	// compressed images. Use the more "correct" source where possible.
	if (kernel->compression == CompressionNone) {
		Arm64KernelHeader *header = kernel->data;
		kernel->load = header->text_offset;
	}

	void *reloc_addr = get_kernel_reloc_addr(kernel->load);
	if (!reloc_addr)
		return 1;

	size_t true_size = kernel->size;
	switch (kernel->compression) {
	case CompressionNone:
		if (kernel->size > MAX_KERNEL_SIZE) {
			printf("ERROR: Cannot relocate a kernel this large!\n");
			return 1;
		}
		printf("Relocating kernel to %p\n", reloc_addr);
		memmove(reloc_addr, kernel->data, kernel->size);
		break;
	case CompressionLzma:
		printf("Decompressing LZMA kernel to %p\n", reloc_addr);
		true_size = ulzman(kernel->data, kernel->size,
				   reloc_addr, MAX_KERNEL_SIZE);
		if (!true_size) {
			printf("ERROR: LZMA decompression failed!\n");
			return 1;
		}
		break;
	case CompressionLz4:
		printf("Decompressing LZ4 kernel to %p\n", reloc_addr);
		true_size = ulz4fn(kernel->data, kernel->size,
				   reloc_addr, MAX_KERNEL_SIZE);
		if (!true_size) {
			printf("ERROR: LZ4 decompression failed!\n");
			return 1;
		}
		break;
	default:
		printf("ERROR: Unsupported compression algorithm!\n");
		return 1;
	}

	Arm64KernelHeader *header = reloc_addr;
	if (header->magic != KERNEL_HEADER_MAGIC) {
		printf("ERROR: Invalid kernel magic: %#.8x\n", header->magic);
		return 1;
	}
	if (header->text_offset != kernel->load) {
		printf("ERROR: FIT load offset did not match kernel header:"
		       "%#.16llx != %#.8x", header->text_offset, kernel->load);
		return 1;
	}

	printf("jumping to kernel\n");

	timestamp_add_now(TS_START_KERNEL);

	/* Flush dcache and icache to make loaded code visible. */
	arch_program_segment_loaded(reloc_addr, true_size);

	tlb_invalidate_all();

	boot_arm_linux_jump(fdt, reloc_addr);

	return 0;
}

/*
 * Copyright (C) 2017 Google Inc.
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

#include <libpayload.h>
#include <stdint.h>
#include <stdlib.h>

#include "base/cleanup_funcs.h"
#include "boot/bootdata.h"
#include "boot/multiboot.h"
#include "config.h"
#include "base/timestamp.h"
#include "vboot/boot_policy.h"
#include "vboot/boot.h"

static const int debug = 0;

// Mini allocator for multiboot parameters
static void *_mb_alloc_end, *_mb_alloc_ptr;

static void mb_alloc_init(void *addr, size_t size)
{
	memset(addr, 0, size);
	_mb_alloc_ptr = addr;
	_mb_alloc_end = addr + size;
}

static void *mb_alloc(size_t length)
{
	void *ptr;
	size_t len = ALIGN_UP(length, sizeof(uint32_t));

	if (_mb_alloc_ptr + len > _mb_alloc_end)
		return NULL;

	ptr = _mb_alloc_ptr;
	_mb_alloc_ptr += len;

	return ptr;
}

static void multiboot_start(struct multiboot_header *header,
			    struct multiboot_info *info)
{
	printf("Starting multiboot kernel @ %p (info @ %p)\n\n",
	       (void *)header->entry_addr, info);

	timestamp_add_now(TS_START_KERNEL);

	__asm__ __volatile__ (
		"cli\n"
		"jmp *%[entry]\n"
		:
		:[entry] "c"(header->entry_addr),
		 "a"(MULTIBOOT_BOOTLOADER_MAGIC),
		 "b"((uintptr_t)info));
}

static struct multiboot_header *multiboot_find(void *start)
{
	struct multiboot_header *header;
	int i;

	if (!start)
		return NULL;

	for (i = 0; i < MULTIBOOT_SEARCH; i += MULTIBOOT_HEADER_ALIGN) {
		header = (struct multiboot_header *)(start + i);
		if (header && header->magic == MULTIBOOT_HEADER_MAGIC &&
		    !(header->magic + header->flags + header->checksum))
			return header;
	}

	printf("%s: unable to find multiboot magic\n", __func__);
	return NULL;
}

static void dump_multiboot_header(struct multiboot_header *header)
{
	if (debug) {
		printf("multiboot header @ %p\n", header);
		printf(" magic            : 0x%08x\n", header->magic);
		printf(" flags            : 0x%08x\n", header->flags);
		printf(" checksum         : 0x%08x\n", header->checksum);
		printf(" header_addr      : 0x%08x\n", header->header_addr);
		printf(" load_addr        : 0x%08x\n", header->load_addr);
		printf(" load_end_addr    : 0x%08x\n", header->load_end_addr);
		printf(" bss_end_addr     : 0x%08x\n", header->bss_end_addr);
		printf(" entry_addr       : 0x%08x\n", header->entry_addr);
	}
}

static void dump_boot_info(struct boot_info *bi)
{
	if (debug) {
		printf("boot_info @ %p\n", bi);
		printf(" bi->kernel       : %p\n", bi->kernel);
		printf(" bi->cmd_line     : %p\n", bi->cmd_line);
		printf(" bi->params       : %p\n", bi->params);
		printf(" bi->ramdisk_addr : %p\n", bi->ramdisk_addr);
		printf(" bi->ramdisk_size : %zu\n", bi->ramdisk_size);
		printf(" bi->loader       : %p\n", bi->loader);
	}
}

int multiboot_fill_boot_info(struct boot_info *bi)
{
	struct multiboot_header *header;

	// Multiboot header should be within the first 8K of kernel image
	header = multiboot_find(bi->kparams->kernel_buffer);
	if (!header)
		return -1;

	// Validate header fields
	if (header->load_addr > header->header_addr || !header->load_end_addr)
		return -1;

	// Require address fields in the header
	if (!(header->flags & MULTIBOOT_AOUT_KLUDGE)) {
		printf("%s: multiboot header does not report addresses\n",
		       __func__);
		return -1;
	}

	dump_multiboot_header(header);

	// Kernel should be loaded at expected offset already by vboot
	if ((uintptr_t)bi->kparams->kernel_buffer !=
	    (uintptr_t)header->load_addr) {
		printf("%s: multiboot kernel not loaded to address %p\n",
		       __func__, (void *)header->load_addr);
		return -1;
	}

	// Point to multiboot header so it can be found again easily
	bi->kernel = header;

	// Use bootloader as ramdisk
	bi->ramdisk_addr = (void *)(uintptr_t)bi->kparams->bootloader_address;
	bi->ramdisk_size = bi->kparams->bootloader_size;

	// Boot parameters come before ramdisk
	bi->params = bi->ramdisk_addr - CrosParamSize;

	// Kernel command line comes before boot parameters
	bi->cmd_line = bi->params - CmdLineSize;

	// Indicate end of BSS as locaion for bootloader to put data
	if (header->bss_end_addr)
		bi->loader = (void *)ALIGN_UP(header->bss_end_addr, 4096);
	else
		bi->loader = (void *)ALIGN_UP(header->load_end_addr, 4096);

	dump_boot_info(bi);

	return 0;
}

/*
 * Example image loaded by vboot and processed by multiboot_fill_boot_info():
 *  00100000:001c3000 kparams->kernel_buffer
 *  00100050:001c3000 bi->kernel (multiboot header)
 *  001c3000:001c4000 bi->cmd_line
 *  001c4000:001c5000 bi->params
 *  001c5000:00503000 bi->ramdisk_addr
 *
 * Multiboot kernel BSS is at end of kernel image so adjust the layout to:
 *  00100000:001c3000 bi->kparams->kernel_buffer
 *  00100050:001c3000 bi->kernel
 *  001c3000:0023c000 bss (zeroed)
 *  0023c000:0023d000 bi->cmd_line
 *  0023d000:0023e000 bi->params
 *  0023e000:0057c000 bi->ramdisk_addr
 */
static int multiboot_load(struct boot_info *bi)
{
	struct multiboot_header *header = bi->kernel;

	// Move params+ramdisk out of kernel BSS region
	memmove(bi->loader + CmdLineSize, bi->params,
		bi->ramdisk_size + CrosParamSize);
	bi->params = bi->loader + CmdLineSize;
	bi->ramdisk_addr = bi->params + CrosParamSize;

	// bi->cmd_line is re-allocated during processing so restore it
	memset(bi->loader, 0, CmdLineSize);
	if (bi->cmd_line)
		strncpy(bi->loader, bi->cmd_line,
			strlen(bi->cmd_line) < CmdLineSize ?
			strlen(bi->cmd_line) : CmdLineSize);
	bi->cmd_line = bi->loader;

	// Clear kernel BSS region
	if (header->bss_end_addr)
		memset((void *)header->load_end_addr, 0,
		       header->bss_end_addr - header->load_end_addr);

	dump_boot_info(bi);

	return 0;
}

static struct multiboot_mmap_entry *multiboot_mmap_create(size_t *count)
{
	struct multiboot_mmap_entry *start, *entry;
	size_t i, c;

	if (lib_sysinfo.n_memranges == 0)
		return NULL;

	start = mb_alloc(sizeof(*entry) * lib_sysinfo.n_memranges);
	if (!start)
		return NULL;

	for (i = c = 0, entry = start; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *range = &lib_sysinfo.memrange[i];

		if (range->size == 0)
			continue;

		entry->size = sizeof(*entry) - sizeof(entry->size);
		entry->addr = range->base;
		entry->len  = range->size;

		switch (range->type) {
		case CB_MEM_RAM:
			entry->type = MULTIBOOT_MEMORY_AVAILABLE;
			break;
		case CB_MEM_NVS:
			entry->type = MULTIBOOT_MEMORY_NVS;
			break;
		case CB_MEM_ACPI:
			entry->type = MULTIBOOT_MEMORY_ACPI_RECLAIMABLE;
			break;
		default:
			entry->type = MULTIBOOT_MEMORY_RESERVED;
			break;
		}

		entry++;
		c++;
	}

	*count = c;
	return start;
}

static struct multiboot_info *multiboot_info(struct boot_info *bi)
{
	struct multiboot_info *info;
	struct multiboot_mmap_entry *mmap;
	size_t mmap_count;

	// Initialize the multiboot info allocator
	mb_alloc_init(bi->params, CrosParamSize);

	info = mb_alloc(sizeof(*info));
	if (!info)
		return NULL;

	// Report command line
	if (bi->cmd_line) {
		info->cmdline = (uint32_t)bi->cmd_line;
		info->flags |= MULTIBOOT_INFO_CMDLINE;
	}

	// Report ramdisk start and size
	if (bi->ramdisk_addr && bi->ramdisk_size) {
		struct multiboot_mod_list *list = mb_alloc(sizeof(*list));
		if (!list)
			return NULL;

		list->mod_start = (uint32_t)bi->ramdisk_addr;
		list->mod_end = (uint32_t)(bi->ramdisk_addr + bi->ramdisk_size);

		info->mods_addr = (uint32_t)list;
		info->mods_count = 1;
		info->flags |= MULTIBOOT_INFO_MODS;
	}

	// Generate multiboot memory map
	mmap = multiboot_mmap_create(&mmap_count);
	if (mmap && mmap_count > 0) {
		info->mmap_addr = (uint32_t)mmap;
		info->mmap_length = mmap_count * sizeof(*mmap);
		info->flags |= MULTIBOOT_INFO_MEM_MAP;
	}

	return info;
}

int multiboot_boot(struct boot_info *bi)
{
	struct multiboot_header *header = bi->kernel;
	struct multiboot_info *info;

	if (!header || header->magic != MULTIBOOT_HEADER_MAGIC) {
		printf("%s: Multiboot header not found at %p\n",
		       __func__, header);
		return -1;
	}

	// Add bootdata info structures if a bootdata kernel is found.
	// This can change bi->ramdisk_size, so do it before setting up
	// the Multiboot structures that describe the ramdisk.
	if (CONFIG(KERNEL_MULTIBOOT_BOOTDATA))
		bootdata_prepare(bi);

	// Load multiboot data into proper locations for boot
	if (multiboot_load(bi) < 0)
		return -1;

	// Prepare the multiboot info structure
	info = multiboot_info(bi);
	if (!info)
		return -1;

	run_cleanup_funcs(CleanupOnHandoff);

	// Start the kernel
	multiboot_start(header, info);

	return 0;
}

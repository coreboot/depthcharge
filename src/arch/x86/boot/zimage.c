/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * (C) Copyright 2002
 * Daniel Engström, Omicron Ceti AB, <daniel@omicron.se>
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

/*
 * Linux x86 zImage and bzImage loading
 *
 * based on the procdure described in
 * linux/Documentation/i386/boot.txt
 */

#include <arch/msr.h>
#include <libpayload.h>

#include "arch/x86/base/cpu.h"
#include "arch/x86/boot/bootparam.h"
#include "arch/x86/boot/zimage.h"
#include "base/timestamp.h"

#define KERNEL_V2_MAGIC		0x53726448
#define MIN_PROTOCOL		0x0202

int board_final_cleanup(void)
{
	/* Un-cache the ROM so the kernel has one
	 * more MTRR available.
	 *
	 * Coreboot should have assigned this to the
	 * top available variable MTRR.
	 */
	uint8_t top_mtrr = (_rdmsr(MTRRcap_MSR) & 0xff) - 1;
	uint8_t top_type = _rdmsr(MTRRphysBase_MSR(top_mtrr)) & 0xff;

	/* Make sure this MTRR is the correct Write-Protected type */
	if (top_type == MTRR_TYPE_WP) {
		disable_cache();
		_wrmsr(MTRRphysBase_MSR(top_mtrr), 0);
		_wrmsr(MTRRphysMask_MSR(top_mtrr), 0);
		enable_cache();
	}

	/* Issue SMI to Coreboot to lock down ME and registers */
	printf("Finalizing Coreboot\n");
	outb(0xcb, 0xb2);

	return 0;
}

int zboot(struct boot_params *setup_base, char *cmd_line, void *kernel_entry)
{
	struct setup_header *hdr = &setup_base->hdr;

	if (hdr->header != KERNEL_V2_MAGIC || hdr->version < MIN_PROTOCOL) {
		printf("Boot protocol is too old.\n");
		return 1;
	}

	unsigned num_entries = MIN(lib_sysinfo.n_memranges,
				   ARRAY_SIZE(setup_base->e820_map));
	if (num_entries < lib_sysinfo.n_memranges) {
		printf("Warning: Limiting e820 map to %d entries.\n",
			num_entries);
	}
	setup_base->e820_entries = num_entries;

	int i;
	for (i = 0; i < num_entries; i++) {
		struct memrange *memrange = &lib_sysinfo.memrange[i];
		struct e820entry *entry = &setup_base->e820_map[i];

		entry->addr = memrange->base;
		entry->size = memrange->size;
		entry->type = memrange->type;
	}

	// Loader type is undefined.
	hdr->type_of_loader = 0xFF;

	hdr->cmd_line_ptr = (uintptr_t)cmd_line;

	puts("Kernel command line: \"");
	puts(cmd_line);
	puts("\"\n");

	puts("\nStarting kernel ...\n\n");

	board_final_cleanup();

	timestamp_add_now(TS_START_KERNEL);

	/*
	 * Set %ebx, %ebp, and %edi to 0, %esi to point to the boot_params
	 * structure, and then jump to the kernel. We assume that %cs is
	 * 0x10, 4GB flat, and read/execute, and the data segments are 0x18,
	 * 4GB flat, and read/write.
	 */
	__asm__ __volatile__ (
	"movl $0, %%ebp		\n"
	"cli			\n"
	"jmp *%[kernel_entry]	\n"
	:: [kernel_entry]"a"(kernel_entry),
	   [boot_params] "S"(setup_base),
	   "b"(0), "D"(0)
	:  "%ebp"
	);
	return 0;
}

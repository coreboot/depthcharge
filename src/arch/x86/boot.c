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

#include <arch/msr.h>
#include <assert.h>
#include <libpayload.h>

#include "arch/x86/boot.h"
#include "arch/x86/cpu.h"
#include "base/cleanup_funcs.h"
#include "base/init_funcs.h"
#include "base/timestamp.h"
#include "vboot/boot.h"

static void * const ParamsBuff = (void *)(uintptr_t)0x1000;
static void * const CmdLineBuff = (void *)(uintptr_t)0x2000;

static const uint32_t KernelV2Magic = 0x53726448;
static const uint16_t MinProtocol = 0x0202;

int boot_x86_linux(struct boot_params *boot_params, char *cmd_line, void *entry)
{
	// Move the boot_params structure and the command line to where Linux
	// suggests and to where they'll be safe from being trampled by the
	// kernel as it's decompressed.
	assert((uint8_t *)CmdLineBuff - (uint8_t *)ParamsBuff >=
		sizeof(*boot_params));
	memcpy(ParamsBuff, boot_params, sizeof(*boot_params));
	boot_params = ParamsBuff;
	strcpy(CmdLineBuff, cmd_line);
	cmd_line = CmdLineBuff;

	struct setup_header *hdr = &boot_params->hdr;

	if (hdr->header != KernelV2Magic || hdr->version < MinProtocol) {
		printf("Boot protocol is too old.\n");
		return 1;
	}

	unsigned num_entries = MIN(lib_sysinfo.n_memranges,
				   ARRAY_SIZE(boot_params->e820_map));
	if (num_entries < lib_sysinfo.n_memranges) {
		printf("Warning: Limiting e820 map to %d entries.\n",
			num_entries);
	}
	boot_params->e820_entries = num_entries;

	int i;
	for (i = 0; i < num_entries; i++) {
		struct memrange *memrange = &lib_sysinfo.memrange[i];
		struct e820entry *entry = &boot_params->e820_map[i];

		entry->addr = memrange->base;
		entry->size = memrange->size;
		entry->type = memrange->type;
	}

	// Loader type is undefined.
	hdr->type_of_loader = 0xFF;

	// Don't reload the data/code segments.
	hdr->loadflags |= KEEP_SEGMENTS;

	hdr->cmd_line_ptr = (uintptr_t)cmd_line;

	run_cleanup_funcs(CleanupOnHandoff);

	puts("\nStarting kernel ...\n\n");
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
	:: [kernel_entry]"a"(entry),
	   [boot_params] "S"(boot_params),
	   "b"(0), "D"(0)
	);
	return 0;
}

static int x86_mtrr_cleanup(struct CleanupFunc *cleanup, CleanupType type)
{
	/*
	 * Un-cache the ROM so the kernel has one more MTRR available.
	 * Coreboot should have assigned this to the top available variable
	 * MTRR.
	 */
	uint8_t top_mtrr = (_rdmsr(MTRRcap_MSR) & 0xff) - 1;
	uint8_t top_type = _rdmsr(MTRRphysBase_MSR(top_mtrr)) & 0xff;

	// Make sure this MTRR is the correct Write-Protected type.
	if (top_type == MTRR_TYPE_WP) {
		disable_cache();
		_wrmsr(MTRRphysBase_MSR(top_mtrr), 0);
		_wrmsr(MTRRphysMask_MSR(top_mtrr), 0);
		enable_cache();
	}
	return 0;
}

static int x86_mtrr_cleanup_install(void)
{
	static CleanupFunc dev =
	{
		&x86_mtrr_cleanup,
		CleanupOnHandoff | CleanupOnLegacy,
		NULL
	};

	list_insert_after(&dev.list_node, &cleanup_funcs);
	return 0;
}

INIT_FUNC(x86_mtrr_cleanup_install);

static int coreboot_finalize(struct CleanupFunc *cleanup, CleanupType type)
{
	// Issue SMI to Coreboot to lock down ME and registers.
	printf("Finalizing Coreboot\n");
	outb(0xcb, 0xb2);
	return 0;
}

static int coreboot_finalize_install(void)
{
	static CleanupFunc dev =
	{
		&coreboot_finalize,
		CleanupOnHandoff | CleanupOnLegacy,
		NULL
	};

	list_insert_after(&dev.list_node, &cleanup_funcs);
	return 0;
}

INIT_FUNC(coreboot_finalize_install);

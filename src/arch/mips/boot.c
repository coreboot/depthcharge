/*
 * Copyright 2015 Google Inc.
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

#include <libpayload.h>

#include "arch/mips/boot.h"
#include "base/cleanup_funcs.h"
#include "base/timestamp.h"

int boot_mips_linux(void *fdt, void *kernel, uint32_t kernel_size)
{
	void (*entry)(unsigned, void *);

	run_cleanup_funcs(CleanupOnHandoff);

	// MIPS kernel is non-PIC - it must start at KERNEL_START.
	printf("Relocating kernel from %p to %#x\n", kernel,
	       CONFIG_KERNEL_START);
	memmove((void *)CONFIG_KERNEL_START, kernel, kernel_size);

	// Kernel expects to start in KSEG0
	entry = phys_to_kseg0(CONFIG_KERNEL_START);
	printf("Starting kernel..\n");

	timestamp_add_now(TS_START_KERNEL);

	cache_sync_instructions();
	entry(0xfffffffe, phys_to_kseg0(virt_to_phys(fdt)));
	printf("Kernel returned!\n");

	return 0;
}

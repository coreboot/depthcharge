/*
 * Copyright 2014 Google Inc.
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

#include <libpayload.h>

#include "base/device_tree.h"
#include "boot/fit.h"
#include "config.h"
#include "vboot/boot.h"

int boot(struct boot_info *bi)
{
	DeviceTree *tree;
	void (*kernel)(int, unsigned, unsigned);
	uint32_t kernel_size;

	if (fit_load(bi->kernel, bi->cmd_line, (void **)(&kernel), &kernel_size,
		     &tree))
		return 1;

	if (!tree) {
		printf("A device tree is required to boot on MIPS.\n");
		return 1;
	}

	if (dt_apply_fixups(tree))
		return 1;

	// Allocate a spot for the FDT in memory.
	void *fdt = (void *)(uintptr_t)CONFIG_KERNEL_FIT_FDT_ADDR;
	uint32_t size = dt_flat_size(tree);

	// Reserve the spot the device tree will go.
	DeviceTreeReserveMapEntry *entry = xzalloc(sizeof(*entry));
	entry->start = (uintptr_t)fdt;
	entry->size = size;
	list_insert_after(&entry->list_node, &tree->reserve_map);

	// Flatten it.
	dt_flatten(tree, fdt);

	/* Relocate and jump to kernel */
	printf("Relocating kernel from %p to %#x\n", kernel,
			CONFIG_KERNEL_START);
	memmove((void *)CONFIG_KERNEL_START, kernel, kernel_size);
	kernel = (void *)CONFIG_KERNEL_START;
	printf("Starting kernel..\n");

	cache_sync_instructions();
	kernel(0, 0xffffffff, virt_to_phys(fdt));
	printf("Kernel returned!\n");

	return 0;
}

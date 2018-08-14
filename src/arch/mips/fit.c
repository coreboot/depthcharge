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
 */

#include <libpayload.h>

#include "arch/mips/boot.h"
#include "base/device_tree.h"
#include "boot/fit.h"
#include "config.h"
#include "image/symbols.h"
#include "vboot/boot.h"

int boot(struct boot_info *bi)
{
	DeviceTree *tree;
	FitImageNode *kernel = fit_load(bi->kernel, bi->cmd_line, &tree);

	if (!kernel || !tree || dt_apply_fixups(tree))
		return 1;

	if (kernel->compression != CompressionNone) {
		printf("Kernel decompression not supported for MIPS.\n");
		return 1;
	}

	// Allocate a spot for the FDT in memory.
	void *fdt = &_fit_fdt_start;
	uint32_t size = dt_flat_size(tree);
	if (&_fit_fdt_start + size > &_fit_fdt_end) {
		printf("ERROR: FDT image overflows buffer!\n");
		return 1;
	}

	// Reserve the spot the device tree will go.
	DeviceTreeReserveMapEntry *entry = xzalloc(sizeof(*entry));
	entry->start = (uintptr_t)fdt;
	entry->size = size;
	list_insert_after(&entry->list_node, &tree->reserve_map);

	// Flatten it.
	dt_flatten(tree, fdt);

	return boot_mips_linux(fdt, kernel->data, kernel->size);
}

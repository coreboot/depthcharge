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
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <coreboot_tables.h>
#include <libpayload.h>
#include <sysinfo.h>

#include "base/device_tree.h"
#include "base/init_funcs.h"

static int install_coreboot_data(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	u32 addr_cells = 1, size_cells = 1;
	const char *firmware_path[] = { "firmware", NULL };
	DeviceTreeNode *firmware_node = dt_find_node(tree->root,
		firmware_path, &addr_cells, &size_cells, 1);

	// Need to add 'ranges' to the intermediate node to make 'reg' work.
	dt_add_bin_prop(firmware_node, "ranges", NULL, 0);

	const char *coreboot_path[] = { "coreboot", NULL };
	DeviceTreeNode *coreboot_node = dt_find_node(firmware_node,
		coreboot_path, &addr_cells, &size_cells, 1);

	dt_add_string_prop(coreboot_node, "compatible", "coreboot");

	struct memrange *cbmem_range = NULL;
	for (int i = lib_sysinfo.n_memranges - 1; i >= 0; i--) {
		if (lib_sysinfo.memrange[i].type == CB_MEM_TABLE) {
			cbmem_range = &lib_sysinfo.memrange[i];
			break;
		}
	}
	if (!cbmem_range)
		return 1;

	u64 reg_addrs[2];
	u64 reg_sizes[2];

	// First 'reg' address range is the coreboot table.
	reg_addrs[0] = (uintptr_t)lib_sysinfo.header;
	reg_sizes[0] = lib_sysinfo.header->header_bytes +
		       lib_sysinfo.header->table_bytes;

	// Second is the CBMEM area (which usually includes the coreboot table).
	reg_addrs[1] = cbmem_range->base;
	reg_sizes[1] = cbmem_range->size;

	dt_add_reg_prop(coreboot_node, reg_addrs, reg_sizes, 2,
			addr_cells, size_cells);

	// Expose board ID, SKU ID, and RAM code exported from coreboot to
	// userspace.
	if (lib_sysinfo.board_id != UNDEFINED_STRAPPING_ID) {
		dt_add_u32_prop(coreboot_node,
				"board-id", lib_sysinfo.board_id);
	}
	if (lib_sysinfo.sku_id != UNDEFINED_STRAPPING_ID) {
		dt_add_u32_prop(coreboot_node,
				"sku-id", lib_sysinfo.sku_id);
	}
	if (lib_sysinfo.ram_code != UNDEFINED_STRAPPING_ID) {
		dt_add_u32_prop(coreboot_node,
				"ram-code", lib_sysinfo.ram_code);
	}

	return 0;
}

static DeviceTreeFixup coreboot_fixup = {
	.fixup = &install_coreboot_data
};

static int coreboot_setup(void)
{
	list_insert_after(&coreboot_fixup.list_node, &device_tree_fixups);
	return 0;
}

INIT_FUNC(coreboot_setup);

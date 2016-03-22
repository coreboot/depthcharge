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
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/device_tree.h"

static int emc_device_tree(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	const char *emc_dt_name[] = { "memory-controller@7001b000", NULL };
	const char *emc_table_dt_name[] = { "emc-table", NULL };
	DeviceTreeNode *emc_node;
	DeviceTreeNode *emc_table_node;

	emc_node = dt_find_node(tree->root, emc_dt_name, NULL, NULL,
				     1);
	if (!emc_node) {
		printf("ERROR: Faied to find node /%s\n",
		       emc_dt_name[0]);
		return 1;
	}

	emc_table_node = dt_find_node(emc_node, emc_table_dt_name, NULL, NULL, 1);
	if (!emc_table_node) {
		printf("ERROR: Failed to find node /%s/%s\n",
		       emc_dt_name[0], emc_table_dt_name[0]);
		return 1;
	}

	dt_add_string_prop(emc_table_node, "compatible", "nvidia,tegra210-emc-table");

	u32 addr_cells = 2, size_cells = 2;

	dt_add_reg_prop(emc_table_node, &lib_sysinfo.mtc_start,
			(u64 *)&lib_sysinfo.mtc_size, 1, addr_cells,
			size_cells);

	DeviceTreeReserveMapEntry *reserve = xzalloc(sizeof(*reserve));
	reserve->start = lib_sysinfo.mtc_start;
	reserve->size = lib_sysinfo.mtc_size;
	list_insert_after(&reserve->list_node, &tree->reserve_map);

	printf("EMC: Added /%s/%s to device-tree\n", emc_dt_name[0],
	       emc_table_dt_name[0]);

	return 0;
}

static DeviceTreeFixup emc_dt_fixup = {
	.fixup = emc_device_tree,
};

static int emc_setup(void)
{
	list_insert_after(&emc_dt_fixup.list_node, &device_tree_fixups);
	return 0;
}

INIT_FUNC(emc_setup);

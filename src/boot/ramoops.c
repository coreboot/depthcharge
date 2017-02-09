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
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <config.h>
#include <endian.h>
#include <libpayload.h>
#include <stdint.h>
#include <string.h>

#include "base/device_tree.h"
#include "base/init_funcs.h"
#include "base/list.h"
#include "vboot/util/memory.h"

typedef struct
{
	uint64_t start;
	uint64_t size;

	DeviceTreeFixup fixup;
} Ramoops;

#define RECORD_SIZE		0x40000
#define CONSOLE_SIZE		0x40000
#define FTRACE_SIZE		0x20000
#define PMSG_SIZE		0x20000

static int ramoops_fixup(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	Ramoops *ramoops = container_of(fixup, Ramoops, fixup);

	DeviceTreeNode *reserved = dt_init_reserved_memory_node(tree);
	if (!reserved)
		return 1;

	// Eliminate any existing ramoops node.
	DeviceTreeNode *node = dt_find_compat(tree->root, "ramoops");
	if (node)
		list_remove(&node->list_node);

	u32 addr_cells = 1, size_cells = 1;
	dt_read_cell_props(reserved, &addr_cells, &size_cells);

	// Create a ramoops node under /reserved-memory/.
	node = xzalloc(sizeof(*node));
	node->name = "ramoops";
	list_insert_after(&node->list_node, &reserved->children);

	// Add a compatible property.
	dt_add_string_prop(node, "compatible", "ramoops");

	// Add a reg property.
	dt_add_reg_prop(node, &ramoops->start, &ramoops->size, 1,
			addr_cells, size_cells);

	// Add size properties.
	dt_add_u32_prop(node, "record-size", RECORD_SIZE);
	dt_add_u32_prop(node, "console-size", CONSOLE_SIZE);
	dt_add_u32_prop(node, "ftrace-size", FTRACE_SIZE);
	dt_add_u32_prop(node, "pmsg-size", PMSG_SIZE);

	// Add the optional dump-oops property.
	dt_add_bin_prop(node, "dump-oops", NULL, 0);

	return 0;
}

static int ramoops_init(void)
{
	static Ramoops ramoops;
	uint64_t start = lib_sysinfo.ramoops_buffer;
	uint64_t size = lib_sysinfo.ramoops_buffer_size;

	if (size == 0)
		return 0;

	ramoops.start = start;
	ramoops.size = size;
	ramoops.fixup.fixup = &ramoops_fixup;
	list_insert_after(&ramoops.fixup.list_node, &device_tree_fixups);

	memory_mark_used(start, start + size);

	return 0;
}

INIT_FUNC(ramoops_init);

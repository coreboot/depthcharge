/*
 * Copyright 2014 Google LLC
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

#include <endian.h>
#include <commonlib/list.h>
#include <libpayload.h>
#include <stdint.h>
#include <string.h>

#include "base/device_tree.h"
#include "base/init_funcs.h"
#include "vboot/util/memory.h"

typedef struct
{
	uint64_t start;
	uint64_t size;

	struct device_tree_fixup fixup;
} Ramoops;

#define RECORD_SIZE		0x40000
#define CONSOLE_SIZE		0x40000
#define FTRACE_SIZE		0x20000
#define PMSG_SIZE		0x20000

static int ramoops_fixup(struct device_tree_fixup *fixup,
			 struct device_tree *tree)
{
	Ramoops *ramoops = container_of(fixup, Ramoops, fixup);

	// Eliminate any existing ramoops node.
	struct device_tree_node *old_node = dt_find_compat(tree->root, "ramoops");
	if (old_node)
		list_remove(&old_node->list_node);

	struct device_tree_node *node = dt_add_reserved_memory_region(tree, "ramoops",
					"ramoops", ramoops->start, ramoops->size, false);

	if (!node)
		return 1;

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

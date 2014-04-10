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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <endian.h>
#include <libpayload.h>
#include <stdint.h>
#include <string.h>

#include "base/device_tree.h"
#include "base/list.h"
#include "boot/ramoops.h"
#include "vboot/util/memory.h"

typedef struct
{
	uint64_t start;
	uint64_t size;
	uint64_t record_size;

	DeviceTreeFixup fixup;
} Ramoops;

static int ramoops_compat(DeviceTreeProperty *prop)
{
	int bytes = prop->prop.size;
	const char *str = prop->prop.data;
	while (bytes && str[0]) {
		if (!strncmp("ramoops", str, bytes))
			return 1;
		int len = strlen(str) + 1;
		str += len;
		bytes -= len;
	}

	return 0;
}

static DeviceTreeNode *find_ramoops_node(DeviceTreeNode *node)
{
	DeviceTreeProperty *prop;
	list_for_each(prop, node->properties, list_node) {
		if (!strcmp("compatible", prop->prop.name)) {
			if (ramoops_compat(prop))
				return node;
			break;
		}
	}

	DeviceTreeNode *child;
	list_for_each(child, node->children, list_node) {
		DeviceTreeNode *ramoops = find_ramoops_node(child);
		if (ramoops)
			return ramoops;
	}

	return NULL;
}

static int ramoops_fixup(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	Ramoops *ramoops = container_of(fixup, Ramoops, fixup);

	uint64_t start = ramoops->start;
	uint64_t size = ramoops->size;
	uint64_t record_size = ramoops->record_size;

	unsigned addr_cells = 1, size_cells = 1;
	dt_node_cell_props(tree->root, &addr_cells, &size_cells);

	if (addr_cells > 2 || size_cells > 2) {
		printf("Bad cell count.\n");
		return -1;
	}

	// Eliminate any existing ramoops node.
	DeviceTreeNode *node = find_ramoops_node(tree->root);
	if (node)
		list_remove(&node->list_node);

	// Create a ramoops node at the root of the tree.
	node = xzalloc(sizeof(*node));
	node->name = "ramoops";
	list_insert_after(&node->list_node, &tree->root->children);

	// Add a compatible property.
	DeviceTreeProperty *compat = xzalloc(sizeof(*compat));
	compat->prop.name = "compatible";
	compat->prop.data = "ramoops";
	compat->prop.size = strlen(compat->prop.data) + 1;
	list_insert_after(&compat->list_node, &node->properties);

	// Add a reg property.
	DeviceTreeProperty *reg = xzalloc(sizeof(*reg));
	reg->prop.name = "reg";
	reg->prop.size = (addr_cells + size_cells) * 4;
	uint8_t *data = xzalloc(reg->prop.size);
	reg->prop.data = data;
	if (addr_cells == 2)
		*(uint64_t *)data = htobell(start);
	else
		*(uint32_t *)data = htobel(start);
	data += addr_cells * 4;
	if (size_cells == 2)
		*(uint64_t *)data = htobell(size);
	else
		*(uint32_t *)data = htobel(size);
	list_insert_after(&reg->list_node, &node->properties);

	// Add a record-size property.
	DeviceTreeProperty *rsize = xzalloc(sizeof(*rsize));
	rsize->prop.name = "record-size";
	rsize->prop.size = size_cells * 4;
	data = xzalloc(rsize->prop.size);
	rsize->prop.data = data;
	if (size_cells == 2)
		*(uint64_t *)data = htobell(record_size);
	else
		*(uint32_t *)data = htobel(record_size);
	list_insert_after(&rsize->list_node, &node->properties);

	// Add the optional dump-oops property.
	DeviceTreeProperty *dump = xzalloc(sizeof(*dump));
	dump->prop.name = "dump-oops";
	list_insert_after(&dump->list_node, &node->properties);

	// Add a memory reservation for the buffer range.
	DeviceTreeReserveMapEntry *reserve = xzalloc(sizeof(*reserve));
	reserve->start = start;
	reserve->size = size;
	list_insert_after(&reserve->list_node, &tree->reserve_map);

	return 0;
}

void ramoops_buffer(uint64_t start, uint64_t size, uint64_t record_size)
{
	static Ramoops ramoops;

	die_if(ramoops.fixup.fixup, "Only one ramoops buffer allowed.\n");

	ramoops.start = start;
	ramoops.size = size;
	ramoops.record_size = record_size;
	ramoops.fixup.fixup = &ramoops_fixup;
	list_insert_after(&ramoops.fixup.list_node, &device_tree_fixups);

	memory_mark_used(start, start + size);
}

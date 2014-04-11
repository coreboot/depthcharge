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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <assert.h>
#include <endian.h>
#include <libpayload.h>
#include <stdint.h>

#include "base/device_tree.h"

/*
 * Functions for picking apart flattened trees.
 */

static uint32_t size32(uint32_t val)
{
	return (val + sizeof(uint32_t) - 1) / sizeof(uint32_t);
}

int fdt_next_property(void *blob, uint32_t offset, FdtProperty *prop)
{
	FdtHeader *header = (FdtHeader *)blob;
	uint32_t *ptr = (uint32_t *)(((uint8_t *)blob) + offset);

	int index = 0;
	if (betohl(ptr[index++]) != TokenProperty)
		return 0;

	uint32_t size = betohl(ptr[index++]);
	uint32_t name_offset = betohl(ptr[index++]);
	name_offset += betohl(header->strings_offset);

	if (prop) {
		prop->name = (char *)((uint8_t *)blob + name_offset);
		prop->data = &ptr[index];
		prop->size = size;
	}

	index += size32(size);

	return index * 4;
}

int fdt_node_name(void *blob, uint32_t offset, const char **name)
{
	uint8_t *ptr = ((uint8_t *)blob) + offset;

	if (betohl(*(uint32_t *)ptr) != TokenBeginNode)
		return 0;

	ptr += 4;
	if (name)
		*name = (char *)ptr;
	return size32(strlen((char *)ptr) + 1) * sizeof(uint32_t) + 4;
}



/*
 * Functions for printing flattened trees.
 */

static void print_indent(int depth)
{
	while (depth--)
		printf("  ");
}

static void print_property(FdtProperty *prop, int depth)
{
	print_indent(depth);
	printf("prop \"%s\" (%d bytes).\n", prop->name, prop->size);
	print_indent(depth + 1);
	for (int i = 0; i < MIN(25, prop->size); i++) {
		printf("%02x ", ((uint8_t *)prop->data)[i]);
	}
	if (prop->size > 25)
		printf("...");
	printf("\n");
}

static int print_flat_node(void *blob, uint32_t start_offset, int depth)
{
	int offset = start_offset;
	const char *name;
	int size;

	size = fdt_node_name(blob, offset, &name);
	if (!size)
		return 0;
	offset += size;

	print_indent(depth);
	printf("name = %s\n", name);

	FdtProperty prop;
	while ((size = fdt_next_property(blob, offset, &prop))) {
		print_property(&prop, depth + 1);

		offset += size;
	}

	while ((size = print_flat_node(blob, offset, depth + 1)))
		offset += size;

	return offset - start_offset + sizeof(uint32_t);
}

void fdt_print_node(void *blob, uint32_t offset)
{
	print_flat_node(blob, offset, 0);
}



/*
 * A utility function to skip past nodes in flattened trees.
 */

int fdt_skip_node(void *blob, uint32_t start_offset)
{
	int offset = start_offset;
	int size;

	const char *name;
	size = fdt_node_name(blob, offset, &name);
	if (!size)
		return 0;
	offset += size;

	while ((size = fdt_next_property(blob, offset, NULL)))
		offset += size;

	while ((size = fdt_skip_node(blob, offset)))
		offset += size;

	return offset - start_offset + sizeof(uint32_t);
}



/*
 * Functions to turn a flattened tree into an unflattened one.
 */

static int fdt_unflatten_node(void *blob, uint32_t start_offset,
			      DeviceTreeNode **new_node)
{
	ListNode *last;
	int offset = start_offset;
	const char *name;
	int size;

	size = fdt_node_name(blob, offset, &name);
	if (!size)
		return 0;
	offset += size;

	DeviceTreeNode *node = xzalloc(sizeof(*node));
	*new_node = node;
	node->name = name;

	FdtProperty fprop;
	last = &node->properties;
	while ((size = fdt_next_property(blob, offset, &fprop))) {
		DeviceTreeProperty *prop = xzalloc(sizeof(*prop));
		prop->prop = fprop;

		list_insert_after(&prop->list_node, last);
		last = &prop->list_node;

		offset += size;
	}

	DeviceTreeNode *child;
	last = &node->children;
	while ((size = fdt_unflatten_node(blob, offset, &child))) {
		list_insert_after(&child->list_node, last);
		last = &child->list_node;

		offset += size;
	}

	return offset - start_offset + sizeof(uint32_t);
}

static int fdt_unflatten_map_entry(void *blob, uint32_t offset,
				   DeviceTreeReserveMapEntry **new_entry)
{
	uint64_t *ptr = (uint64_t *)(((uint8_t *)blob) + offset);
	uint64_t start = betohll(ptr[0]);
	uint64_t size = betohll(ptr[1]);

	if (!size)
		return 0;

	DeviceTreeReserveMapEntry *entry = xzalloc(sizeof(*entry));
	*new_entry = entry;
	entry->start = start;
	entry->size = size;

	return sizeof(uint64_t) * 2;
}

DeviceTree *fdt_unflatten(void *blob)
{
	DeviceTree *tree = xzalloc(sizeof(*tree));
	FdtHeader *header = (FdtHeader *)blob;
	tree->header = header;

	uint32_t struct_offset = betohl(header->structure_offset);
	uint32_t strings_offset = betohl(header->strings_offset);
	uint32_t reserve_offset = betohl(header->reserve_map_offset);
	uint32_t min_offset = 0;
	min_offset = MIN(struct_offset, strings_offset);
	min_offset = MIN(min_offset, reserve_offset);
	// Assume everything up to the first non-header component is part of
	// the header and needs to be preserved. This will protect us against
	// new elements being added in the future.
	tree->header_size = min_offset;

	DeviceTreeReserveMapEntry *entry;
	uint32_t offset = reserve_offset;
	int size;
	ListNode *last = &tree->reserve_map;
	while ((size = fdt_unflatten_map_entry(blob, offset, &entry))) {
		list_insert_after(&entry->list_node, last);
		last = &entry->list_node;

		offset += size;
	}

	fdt_unflatten_node(blob, struct_offset, &tree->root);

	return tree;
}



/*
 * Functions to find the size of device tree would take if it was flattened.
 */

static void dt_flat_prop_size(DeviceTreeProperty *prop, uint32_t *struct_size,
			      uint32_t *strings_size)
{
	// Starting token.
	*struct_size += sizeof(uint32_t);
	// Size.
	*struct_size += sizeof(uint32_t);
	// Name offset.
	*struct_size += sizeof(uint32_t);
	// Property value.
	*struct_size += size32(prop->prop.size) * sizeof(uint32_t);

	// Property name.
	*strings_size += strlen(prop->prop.name) + 1;
}

static void dt_flat_node_size(DeviceTreeNode *node, uint32_t *struct_size,
			      uint32_t *strings_size)
{
	// Starting token.
	*struct_size += sizeof(uint32_t);
	// Node name.
	*struct_size += size32(strlen(node->name) + 1) * sizeof(uint32_t);

	DeviceTreeProperty *prop;
	list_for_each(prop, node->properties, list_node)
		dt_flat_prop_size(prop, struct_size, strings_size);

	DeviceTreeNode *child;
	list_for_each(child, node->children, list_node)
		dt_flat_node_size(child, struct_size, strings_size);

	// End token.
	*struct_size += sizeof(uint32_t);
}

uint32_t dt_flat_size(DeviceTree *tree)
{
	uint32_t size = tree->header_size;
	DeviceTreeReserveMapEntry *entry;
	list_for_each(entry, tree->reserve_map, list_node)
		size += sizeof(uint64_t) * 2;
	size += sizeof(uint64_t) * 2;

	uint32_t struct_size = 0;
	uint32_t strings_size = 0;
	dt_flat_node_size(tree->root, &struct_size, &strings_size);

	size += struct_size;
	// End token.
	size += sizeof(uint32_t);

	size += strings_size;

	return size;
}



/*
 * Functions to flatten a device tree.
 */

static void dt_flatten_map_entry(DeviceTreeReserveMapEntry *entry,
				 void **map_start)
{
	((uint64_t *)*map_start)[0] = htobell(entry->start);
	((uint64_t *)*map_start)[1] = htobell(entry->size);
	*map_start = ((uint8_t *)*map_start) + sizeof(uint64_t) * 2;
}

static void dt_flatten_prop(DeviceTreeProperty *prop, void **struct_start,
			    void *strings_base, void **strings_start)
{
	uint8_t *dstruct = (uint8_t *)*struct_start;
	uint8_t *dstrings = (uint8_t *)*strings_start;

	*((uint32_t *)dstruct) = htobel(TokenProperty);
	dstruct += sizeof(uint32_t);

	*((uint32_t *)dstruct) = htobel(prop->prop.size);
	dstruct += sizeof(uint32_t);

	uint32_t name_offset = (uintptr_t)dstrings - (uintptr_t)strings_base;
	*((uint32_t *)dstruct) = htobel(name_offset);
	dstruct += sizeof(uint32_t);

	strcpy((char *)dstrings, prop->prop.name);
	dstrings += strlen(prop->prop.name) + 1;

	memcpy(dstruct, prop->prop.data, prop->prop.size);
	dstruct += size32(prop->prop.size) * 4;

	*struct_start = dstruct;
	*strings_start = dstrings;
}

static void dt_flatten_node(DeviceTreeNode *node, void **struct_start,
			    void *strings_base, void **strings_start)
{
	uint8_t *dstruct = (uint8_t *)*struct_start;
	uint8_t *dstrings = (uint8_t *)*strings_start;

	*((uint32_t *)dstruct) = htobel(TokenBeginNode);
	dstruct += sizeof(uint32_t);

	strcpy((char *)dstruct, node->name);
	dstruct += size32(strlen(node->name) + 1) * 4;

	DeviceTreeProperty *prop;
	list_for_each(prop, node->properties, list_node)
		dt_flatten_prop(prop, (void **)&dstruct, strings_base,
				(void **)&dstrings);

	DeviceTreeNode *child;
	list_for_each(child, node->children, list_node)
		dt_flatten_node(child, (void **)&dstruct, strings_base,
				(void **)&dstrings);

	*((uint32_t *)dstruct) = htobel(TokenEndNode);
	dstruct += sizeof(uint32_t);

	*struct_start = dstruct;
	*strings_start = dstrings;
}

void dt_flatten(DeviceTree *tree, void *start_dest)
{
	uint8_t *dest = (uint8_t *)start_dest;

	memcpy(dest, tree->header, tree->header_size);
	FdtHeader *header = (FdtHeader *)dest;
	dest += tree->header_size;

	DeviceTreeReserveMapEntry *entry;
	list_for_each(entry, tree->reserve_map, list_node)
		dt_flatten_map_entry(entry, (void **)&dest);
	((uint64_t *)dest)[0] = ((uint64_t *)dest)[1] = 0;
	dest += sizeof(uint64_t) * 2;

	uint32_t struct_size = 0;
	uint32_t strings_size = 0;
	dt_flat_node_size(tree->root, &struct_size, &strings_size);

	uint8_t *struct_start = dest;
	header->structure_offset = htobel(dest - (uint8_t *)start_dest);
	header->structure_size = htobel(struct_size);
	dest += struct_size;

	*((uint32_t *)dest) = htobel(TokenEnd);
	dest += sizeof(uint32_t);

	uint8_t *strings_start = dest;
	header->strings_offset = htobel(dest - (uint8_t *)start_dest);
	header->strings_size = htobel(strings_size);
	dest += strings_size;

	dt_flatten_node(tree->root, (void **)&struct_start, strings_start,
			(void **)&strings_start);

	header->totalsize = htobel(dest - (uint8_t *)start_dest);
}



/*
 * Functions for printing a non-flattened device tree.
 */

static void print_node(DeviceTreeNode *node, int depth)
{
	print_indent(depth);
	printf("name = %s\n", node->name);

	DeviceTreeProperty *prop;
	list_for_each(prop, node->properties, list_node)
		print_property(&prop->prop, depth + 1);

	DeviceTreeNode *child;
	list_for_each(child, node->children, list_node)
		print_node(child, depth + 1);
}

void dt_print_node(DeviceTreeNode *node)
{
	print_node(node, 0);
}



/*
 * Utility function for finding #address-cells and #size-cells properties
 * in a node.
 */

void dt_node_cell_props(DeviceTreeNode *node, unsigned *addr, unsigned *size)
{
	DeviceTreeProperty *prop;
	list_for_each(prop, node->properties, list_node) {
		if (!strcmp("#address-cells", prop->prop.name))
			*addr = betohl(*(uint32_t *)prop->prop.data);
		if (!strcmp("#size-cells", prop->prop.name))
			*size = betohl(*(uint32_t *)prop->prop.data);
	}
}



/*
 * Fixups to apply to a kernel's device tree before booting it.
 */

ListNode device_tree_fixups;

int dt_apply_fixups(DeviceTree *tree)
{
	DeviceTreeFixup *fixup;
	list_for_each(fixup, device_tree_fixups, list_node) {
		assert(fixup->fixup);
		if (fixup->fixup(fixup, tree))
			return 1;
	}
	return 0;
}

/*
 * Copyright 2013 Google LLC
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
	printf("%*s", depth * 8, "");
}

static void print_property(FdtProperty *prop, int depth)
{
	int is_string = prop->size > 0 &&
			((char *)prop->data)[prop->size - 1] == '\0';

	if (is_string)
		for (char *c = prop->data; *c != '\0'; c++)
			if (!isprint(*c))
				is_string = 0;

	print_indent(depth);
	if (is_string) {
		printf("%s = \"%s\";\n", prop->name, (char *)prop->data);
	} else {
		printf("%s = < ", prop->name);
		for (int i = 0; i < MIN(128, prop->size); i += 4) {
			uint32_t val = 0;
			for (int j = 0; j < MIN(4, prop->size - i); j++)
				val |= ((uint8_t *)prop->data)[i + j] <<
					(24 - j * 8);
			printf("%#.2x ", val);
		}
		if (prop->size > 128)
			printf("...");
		printf(">;\n");
	}
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
	printf("%s {\n", name);

	FdtProperty prop;
	while ((size = fdt_next_property(blob, offset, &prop))) {
		print_property(&prop, depth + 1);

		offset += size;
	}

	printf("\n");	// empty line between props and nodes

	while ((size = print_flat_node(blob, offset, depth + 1)))
		offset += size;

	print_indent(depth);
	printf("}\n");

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

static DeviceTreeNode node_cache[1000];
static int node_counter = 0;
static DeviceTreeProperty prop_cache[5000];
static int prop_counter = 0;

/*
 * Libpayload's malloc() has linear allocation complexity and goes completely
 * mental after a few thousand small requests. This little hack will absorb
 * the worst of it to avoid increasing boot time for no reason.
 */
static DeviceTreeNode *alloc_node(void)
{
	if (node_counter >= ARRAY_SIZE(node_cache))
		return xzalloc(sizeof(DeviceTreeNode));
	return &node_cache[node_counter++];
}
static DeviceTreeProperty *alloc_prop(void)
{
	if (prop_counter >= ARRAY_SIZE(prop_cache))
		return xzalloc(sizeof(DeviceTreeProperty));
	return &prop_cache[prop_counter++];
}

static int dt_prop_is_phandle(DeviceTreeProperty *prop)
{
	return !(strcmp("phandle", prop->prop.name) &&
		 strcmp("linux,phandle", prop->prop.name));
}

static int fdt_unflatten_node(void *blob, uint32_t start_offset,
			      DeviceTree *tree, DeviceTreeNode **new_node)
{
	struct list_node *last;
	int offset = start_offset;
	const char *name;
	int size;

	size = fdt_node_name(blob, offset, &name);
	if (!size)
		return 0;
	offset += size;

	DeviceTreeNode *node = alloc_node();
	*new_node = node;
	node->name = name;

	FdtProperty fprop;
	last = &node->properties;
	while ((size = fdt_next_property(blob, offset, &fprop))) {
		DeviceTreeProperty *prop = alloc_prop();
		prop->prop = fprop;

		if (dt_prop_is_phandle(prop)) {
			node->phandle = be32dec(prop->prop.data);
			if (node->phandle > tree->max_phandle)
				tree->max_phandle = node->phandle;
		}

		list_insert_after(&prop->list_node, last);
		last = &prop->list_node;

		offset += size;
	}

	DeviceTreeNode *child;
	last = &node->children;
	while ((size = fdt_unflatten_node(blob, offset, tree, &child))) {
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

	uint32_t magic = betohl(header->magic);
	uint32_t version = betohl(header->version);
	uint32_t last_comp_version = betohl(header->last_comp_version);

	if (magic != FdtMagic) {
		printf("Invalid device tree magic %#.8x!\n", magic);
		return NULL;
	}
	if (last_comp_version > FdtSupportedVersion) {
		printf("Unsupported device tree version %u(>=%u)!\n",
		       version, last_comp_version);
		return NULL;
	}
	if (version > FdtSupportedVersion)
		printf("NOTE: FDT version %u too new, should add support!\n",
		       version);

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
	struct list_node *last = &tree->reserve_map;
	while ((size = fdt_unflatten_map_entry(blob, offset, &entry))) {
		list_insert_after(&entry->list_node, last);
		last = &entry->list_node;

		offset += size;
	}

	fdt_unflatten_node(blob, struct_offset, tree, &tree->root);

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
	be32enc(&dest[struct_size], TokenEnd);
	struct_size += sizeof(uint32_t);
	header->structure_size = htobel(struct_size);
	dest += struct_size;

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
	if (depth == 0)
		printf("/");	// root node has no name, print a starting slash
	printf("%s {\n", node->name);

	DeviceTreeProperty *prop;
	list_for_each(prop, node->properties, list_node)
		print_property(&prop->prop, depth + 1);

	printf("\n");	// empty line between props and nodes

	DeviceTreeNode *child;
	list_for_each(child, node->children, list_node)
		print_node(child, depth + 1);

	print_indent(depth);
	printf("};\n");
}

void dt_print_node(DeviceTreeNode *node)
{
	print_node(node, 0);
}



/*
 * Functions for reading and manipulating an unflattened device tree.
 */

/*
 * Read #address-cells and #size-cells properties from a node.
 *
 * @param node		The device tree node to read from.
 * @param addrcp	Pointer to store #address-cells in, skipped if NULL.
 * @param sizecp	Pointer to store #size-cells in, skipped if NULL.
 */
void dt_read_cell_props(DeviceTreeNode *node, u32 *addrcp, u32 *sizecp)
{
	DeviceTreeProperty *prop;
	list_for_each(prop, node->properties, list_node) {
		if (addrcp && !strcmp("#address-cells", prop->prop.name))
			*addrcp = betohl(*(u32 *)prop->prop.data);
		if (sizecp && !strcmp("#size-cells", prop->prop.name))
			*sizecp = betohl(*(u32 *)prop->prop.data);
	}
}

/*
 * Find a node from a device tree path, relative to a parent node.
 *
 * @param parent	The node from which to start the relative path lookup.
 * @param path		An array of path component strings that will be looked
 * 			up in order to find the node. Must be terminated with
 * 			a NULL pointer. Example: {'firmware', 'coreboot', NULL}
 * @param addrcp	Pointer that will be updated with any #address-cells
 * 			value found in the path. May be NULL to ignore.
 * @param sizecp	Pointer that will be updated with any #size-cells
 * 			value found in the path. May be NULL to ignore.
 * @param create	1: Create node(s) if not found. 0: Return NULL instead.
 * @return		The found/created node, or NULL.
 */
DeviceTreeNode *dt_find_node(DeviceTreeNode *parent, const char **path,
			     u32 *addrcp, u32 *sizecp, int create)
{
	DeviceTreeNode *node, *found = NULL;

	// Update #address-cells and #size-cells for this level.
	dt_read_cell_props(parent, addrcp, sizecp);

	if (!*path)
		return parent;

	// Find the next node in the path, if it exists.
	list_for_each(node, parent->children, list_node) {
		if (!strcmp(node->name, *path)) {
			found = node;
			break;
		}
	}

	// Otherwise create it or return NULL.
	if (!found) {
		if (!create)
			return NULL;

		found = alloc_node();
		found->name = strdup(*path);
		if (!found->name)
			return NULL;

		list_insert_after(&found->list_node, &parent->children);
	}

	return dt_find_node(found, path + 1, addrcp, sizecp, create);
}

/*
 * Find a node from a device tree path string.
 *
 * @param tree		The device tree object
 * @param path          A string representing a path in the device tree, with
 *			nodes separated by '/'.
 *			Example: "/soc/firmware/coreboot"
 * @param addrcp	Pointer that will be updated with any #address-cells
 *			value found in the path. May be NULL to ignore.
 * @param sizecp	Pointer that will be updated with any #size-cells
 *			value found in the path. May be NULL to ignore.
 * @param create	1: Create node(s) if not found. 0: Return NULL instead.
 * @return		The found/created node, or NULL.
 *
 * It is the caller responsibility to provide the correct path string, namely
 * starting with '/' for a full path, not starting with '/' for a path
 * with an alias, not ending with a '/', and not having "//" anywhere in it.
 */
DeviceTreeNode *dt_find_node_by_path(DeviceTree *tree, const char *path,
				     u32 *addrcp, u32 *sizecp, int create)
{
	char *sub_path;
	char *duped_str;
	DeviceTreeNode *parent;
	char *next_slash;
	/* Hopefully enough depth for any node. */
	const char *path_array[15];
	int i;
	DeviceTreeNode *node = NULL;

	if (path[0] == '/') { // regular path
		if (path[1] == '\0') {	// special case: "/" is root node
			dt_read_cell_props(tree->root, addrcp, sizecp);
			return tree->root;
		}

		sub_path = duped_str = strdup(&path[1]);
		if (!sub_path)
			return NULL;

		parent = tree->root;
	} else { // alias
		char *alias;

		alias = duped_str = strdup(path);
		if (!alias)
			return NULL;

		sub_path = strchr(alias, '/');
		if (sub_path)
			*sub_path = '\0';

		parent = dt_find_node_by_alias(tree, alias);
		if (!parent) {
			printf("Could not find node '%s', alias '%s' does not exist\n",
			       path, alias);
			free(duped_str);
			return NULL;
		}

		if (!sub_path) {
			// it's just the alias, no sub-path
			free(duped_str);
			return parent;
		}

		sub_path++;
	}

	next_slash = sub_path;
	path_array[0] = sub_path;
	for (i = 1; i < (ARRAY_SIZE(path_array) - 1); i++) {
		next_slash = strchr(next_slash, '/');
		if (!next_slash)
			break;

		*next_slash++ = '\0';
		path_array[i] = next_slash;
	}

	if (!next_slash) {
		path_array[i] = NULL;
		node = dt_find_node(parent, path_array,
				    addrcp, sizecp, create);
	}

	free(duped_str);
	return node;
}

DeviceTreeNode *dt_find_node_by_phandle(DeviceTreeNode *root, uint32_t phandle)
{
	if (!root)
		return NULL;

	if (root->phandle == phandle)
		return root;

	DeviceTreeNode *node;
	DeviceTreeNode *result;
	list_for_each(node, root->children, list_node) {
		result = dt_find_node_by_phandle(node, phandle);
		if (result)
			return result;
	}

	return NULL;
}

/*
 * Find a node from an alias
 *
 * @param tree		The device tree.
 * @param alias		The alias name.
 * @return		The found node, or NULL.
 */
DeviceTreeNode *dt_find_node_by_alias(DeviceTree *tree, const char *alias)
{
	DeviceTreeNode *node;
	const char *alias_path;

	node = dt_find_node_by_path(tree, "/aliases", NULL, NULL, 0);
	if (!node)
		return NULL;

	alias_path = dt_find_string_prop(node, alias);
	if (!alias_path)
		return NULL;

	return dt_find_node_by_path(tree, alias_path, NULL, NULL, 0);
}

/*
 * Check if given node is compatible.
 *
 * @param node		The node which is to be checked for compatible property.
 * @param compat	The compatible string to match.
 * @return		1 = compatible, 0 = not compatible.
 */
static int dt_check_compat_match(DeviceTreeNode *node, const char *compat)
{
	DeviceTreeProperty *prop;

	list_for_each(prop, node->properties, list_node) {
		if (!strcmp("compatible", prop->prop.name)) {
			size_t bytes = prop->prop.size;
			const char *str = prop->prop.data;
			while (bytes > 0) {
				if (!strncmp(compat, str, bytes))
					return 1;
				size_t len = strnlen(str, bytes) + 1;
				if (bytes <= len)
					break;
				str += len;
				bytes -= len;
			}
			break;
		}
	}

	return 0;
}

/*
 * Find a node from a compatible string, in the subtree of a parent node.
 *
 * @param parent	The parent node under which to look.
 * @param compat	The compatible string to find.
 * @return		The found node, or NULL.
 */
DeviceTreeNode *dt_find_compat(DeviceTreeNode *parent, const char *compat)
{
	// Check if the parent node itself is compatible.
	if (dt_check_compat_match(parent, compat))
		return parent;

	DeviceTreeNode *child;
	list_for_each(child, parent->children, list_node) {
		DeviceTreeNode *found = dt_find_compat(child, compat);
		if (found)
			return found;
	}

	return NULL;
}

/*
 * Find the next compatible child of a given parent. All children upto the
 * child passed in by caller are ignored. If child is NULL, it considers all the
 * children to find the first child which is compatible.
 *
 * @param parent	The parent node under which to look.
 * @param child	The child node to start search from (exclusive). If NULL
 *                      consider all children.
 * @param compat	The compatible string to find.
 * @return		The found node, or NULL.
 */
DeviceTreeNode *dt_find_next_compat_child(DeviceTreeNode *parent,
					  DeviceTreeNode *child,
					  const char *compat)
{
	DeviceTreeNode *next;
	int ignore = 0;

	if (child)
		ignore = 1;

	list_for_each(next, parent->children, list_node) {
		if (ignore) {
			if (child == next)
				ignore = 0;
			continue;
		}

		if (dt_check_compat_match(next, compat))
			return next;
	}

	return NULL;
}

/*
 * Find a node with matching property value, in the subtree of a parent node.
 *
 * @param parent	The parent node under which to look.
 * @param name		The property name to look for.
 * @param data		The property value to look for.
 * @param size		The property size.
 */
DeviceTreeNode *dt_find_prop_value(DeviceTreeNode *parent, const char *name,
				   void *data, size_t size)
{
	DeviceTreeProperty *prop;

	/* Check if parent itself has the required property value. */
	list_for_each(prop, parent->properties, list_node) {
		if (!strcmp(name, prop->prop.name)) {
			size_t bytes = prop->prop.size;
			void *prop_data = prop->prop.data;
			if (size != bytes)
				break;
			if (!memcmp(data, prop_data, size))
				return parent;
			break;
		}
	}

	DeviceTreeNode *child;
	list_for_each(child, parent->children, list_node) {
		DeviceTreeNode *found = dt_find_prop_value(child, name, data,
							   size);
		if (found)
			return found;
	}
	return NULL;
}

/*
 * Write an arbitrary sized big-endian integer into a pointer.
 *
 * @param dest		Pointer to the DT property data buffer to write.
 * @param src		The integer to write (in CPU endianess).
 * @param length	the length of the destination integer in bytes.
 */
void dt_write_int(u8 *dest, u64 src, size_t length)
{
	while (length--) {
		dest[length] = (u8)src;
		src >>= 8;
	}
}

/*
 * Add an arbitrary property to a node, or update it if it already exists.
 *
 * @param node		The device tree node to add to.
 * @param name		The name of the new property.
 * @param data		The raw data blob to be stored in the property.
 * @param size		The size of data in bytes.
 */
void dt_add_bin_prop(DeviceTreeNode *node, const char *name, void *data,
		     size_t size)
{
	DeviceTreeProperty *prop;

	list_for_each(prop, node->properties, list_node) {
		if (!strcmp(prop->prop.name, name)) {
			prop->prop.data = data;
			prop->prop.size = size;
			return;
		}
	}

	prop = alloc_prop();
	list_insert_after(&prop->list_node, &node->properties);
	prop->prop.name = name;
	prop->prop.data = data;
	prop->prop.size = size;
}

/*
 * Find given string property in a node and return its content.
 *
 * @param node		The device tree node to search.
 * @param name		The name of the property.
 * @return		The found string, or NULL.
 */
const char *dt_find_string_prop(DeviceTreeNode *node, const char *name)
{
	void *content;
	size_t size;

	dt_find_bin_prop(node, name, &content, &size);

	return content;
}

/*
 * Find given property in a node.
 *
 * @param node		The device tree node to search.
 * @param name		The name of the property.
 * @param data		Pointer to return raw data blob in the property.
 * @param size		Pointer to return the size of data in bytes.
 */
void dt_find_bin_prop(DeviceTreeNode *node, const char *name, void **data,
		      size_t *size)
{
	DeviceTreeProperty *prop;

	*data = NULL;
	*size = 0;

	list_for_each(prop, node->properties, list_node) {
		if (!strcmp(prop->prop.name, name)) {
			*data = prop->prop.data;
			*size = prop->prop.size;
			return;
		}
	}
}

/*
 * Add a string property to a node, or update it if it already exists.
 *
 * @param node		The device tree node to add to.
 * @param name		The name of the new property.
 * @param str		The zero-terminated string to be stored in the property.
 */
void dt_add_string_prop(DeviceTreeNode *node, const char *name, char *str)
{
	dt_add_bin_prop(node, name, str, strlen(str) + 1);
}

/*
 * Remove a property from a node.
 *
 * @param node		The device tree node to remove from.
 * @param name		The name of the property.
 */
void dt_remove_prop(DeviceTreeNode *node, const char *name)
{
	DeviceTreeProperty *prop;

	list_for_each(prop, node->properties, list_node) {
		if (!strcmp(prop->prop.name, name)) {
			list_remove(&prop->list_node);
			return;
		}
	}
}

/*
 * Add a 32-bit integer property to a node, or update it if it already exists.
 *
 * @param node		The device tree node to add to.
 * @param name		The name of the new property.
 * @param val		The integer to be stored in the property.
 */
void dt_add_u32_prop(DeviceTreeNode *node, const char *name, u32 val)
{
	u32 *val_ptr = xmalloc(sizeof(val));
	*val_ptr = htobel(val);
	dt_add_bin_prop(node, name, val_ptr, sizeof(*val_ptr));
}

/*
 * Add a 64-bit integer property to a node, or update it if it already exists.
 *
 * @param node		The device tree node to add to.
 * @param name		The name of the new property.
 * @param val		The integer to be stored in the property.
 */
void dt_add_u64_prop(DeviceTreeNode *node, const char *name, u64 val)
{
	u64 *val_ptr = xmalloc(sizeof(val));
	*val_ptr = htobell(val);
	dt_add_bin_prop(node, name, val_ptr, sizeof(*val_ptr));
}

/*
 * Add a 'reg' address list property to a node, or update it if it exists.
 *
 * @param node		The device tree node to add to.
 * @param addrs		Array of address values to be stored in the property.
 * @param sizes		Array of corresponding size values to 'addrs'.
 * @param count		Number of values in 'addrs' and 'sizes' (must be equal).
 * @param addr_cells	Value of #address-cells property valid for this node.
 * @param size_cells	Value of #size-cells property valid for this node.
 */
void dt_add_reg_prop(DeviceTreeNode *node, u64 *addrs, u64 *sizes,
		     int count, u32 addr_cells, u32 size_cells)
{
	int i;
	size_t length = (addr_cells + size_cells) * sizeof(u32) * count;
	u8 *data = xmalloc(length);
	u8 *cur = data;

	for (i = 0; i < count; i++) {
		dt_write_int(cur, addrs[i], addr_cells * sizeof(u32));
		cur += addr_cells * sizeof(u32);
		dt_write_int(cur, sizes[i], size_cells * sizeof(u32));
		cur += size_cells * sizeof(u32);
	}

	dt_add_bin_prop(node, "reg", data, length);
}

/*
 * Fixups to apply to a kernel's device tree before booting it.
 */

struct list_node device_tree_fixups;

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

int dt_set_bin_prop_by_path(DeviceTree *tree, const char *path,
			    void *data, size_t data_size, int create)
{
	char *path_copy, *prop_name;
	DeviceTreeNode *dt_node;

	path_copy = strdup(path);

	if (!path_copy) {
		printf("Failed to allocate a copy of path %s\n", path);
		return 1;
	}

	prop_name = strrchr(path_copy, '/');
	if (!prop_name) {
		printf("Path %s does not include '/'\n", path);
		free(path_copy);
		return 1;
	}

	*prop_name++ = '\0'; /* Separate path from the property name. */

	dt_node = dt_find_node_by_path(tree, path_copy, NULL, NULL, create);

	if (!dt_node) {
		printf("Failed to %s %s in the device tree\n",
		       create ? "create" : "find", path_copy);
		free(path_copy);
		return 1;
	}

	dt_add_bin_prop(dt_node, prop_name, data, data_size);

	return 0;
}

/*
 * Prepare the /reserved-memory/ node.
 *
 * Technically, this can be called more than one time, to init and/or retrieve
 * the node. But dt_add_u32_prop() may leak a bit of memory if you do.
 *
 * @tree: Device tree to add/retrieve from.
 * @return: The /reserved-memory/ node (or NULL, if error).
 */
DeviceTreeNode *dt_init_reserved_memory_node(DeviceTree *tree)
{
	DeviceTreeNode *reserved;
	u32 addr = 0, size = 0;

	reserved = dt_find_node_by_path(tree, "/reserved-memory", &addr,
					&size, 1);
	if (!reserved)
		return NULL;

	// Binding doc says this should have the same #{address,size}-cells as
	// the root.
	dt_add_u32_prop(reserved, "#address-cells", addr);
	dt_add_u32_prop(reserved, "#size-cells", size);
	// Binding doc says this should be empty (i.e., 1:1 mapping from root).
	dt_add_bin_prop(reserved, "ranges", NULL, 0);

	return reserved;
}

/*
 * Increment a single phandle in prop at a given offset by a given adjustment.
 *
 * @param prop		Property whose phandle should be adjusted.
 * @param adjustment	Value that should be added to the existing phandle.
 * @param offset	Byte offset of the phandle in the property data.
 *
 * @return		New phandle value, or 0 on error.
 */
static uint32_t dt_adjust_phandle(DeviceTreeProperty *prop,
				  uint32_t adjustment, uint32_t offset)
{
	if (offset + 4 > prop->prop.size)
		return 0;

	uint32_t phandle = be32dec(prop->prop.data + offset);
	if (phandle == 0 ||
	    phandle == PhandleIllegal ||
	    phandle == 0xffffffff)
		return 0;

	phandle += adjustment;
	if(phandle >= PhandleIllegal)
		return 0;

	be32enc(prop->prop.data + offset, phandle);
	return phandle;
}

/*
 * Adjust all phandles in subtree by adding a new base offset.
 *
 * @param node		Root node of the subtree to work on.
 * @param base		New phandle base to be added to all phandles.
 *
 * @return		New highest phandle in the subtree, or 0 on error.
 */
static uint32_t dt_adjust_all_phandles(DeviceTreeNode *node, uint32_t base)
{
	uint32_t new_max = MAX(base, 1);  // make sure we don't return 0
	DeviceTreeProperty *prop;
	DeviceTreeNode *child;

	if (!node)
		return new_max;

	list_for_each(prop, node->properties, list_node)
		if (dt_prop_is_phandle(prop)) {
			node->phandle = dt_adjust_phandle(prop, base, 0);
			if (!node->phandle)
				return 0;
			new_max = MAX(new_max, node->phandle);
		}  // no break -- can have more than one phandle prop

	list_for_each(child, node->children, list_node)
		new_max = MAX(new_max, dt_adjust_all_phandles(child, base));

	return new_max;
}

/*
 * Apply a /__local_fixup__ subtree to the corresponding overlay subtree.
 *
 * @param node		Root node of the overlay subtree to fix up.
 * @param node		Root node of the /__local_fixup__ subtree.
 * @param base		Adjustment that was added to phandles in the overlay.
 *
 * @return		0 on success, -1 on error.
 */
int dt_fixup_locals(DeviceTreeNode *node, DeviceTreeNode *fixup, uint32_t base)
{
	DeviceTreeProperty *prop;
	DeviceTreeProperty *fixup_prop;
	DeviceTreeNode *child;
	DeviceTreeNode *fixup_child;
	int i;

	// For local fixups the /__local_fixup__ subtree contains the same node
	// hierarchy as the main tree we're fixing up. Each property contains
	// the fixup offsets for the respective property in the main tree. For
	// each property in the fixup node, find the corresponding property in
	// the base node and apply fixups to all offsets it specifies.
	list_for_each(fixup_prop, fixup->properties, list_node) {
		DeviceTreeProperty *base_prop = NULL;
		list_for_each(prop, node->properties, list_node)
			if (!strcmp(prop->prop.name, fixup_prop->prop.name)) {
				base_prop = prop;
				break;
			}

		// We should always find a corresponding base prop for a fixup,
		// and fixup props contain a list of 32-bit fixup offsets.
		if (!base_prop || fixup_prop->prop.size % sizeof(uint32_t))
			return -1;

		for (i = 0; i < fixup_prop->prop.size; i += sizeof(uint32_t))
			if (!dt_adjust_phandle(base_prop, base,
					be32dec(fixup_prop->prop.data + i)))
				return -1;
	}

	// Now recursively descend both the base tree and the /__local_fixups__
	// subtree in sync to apply all fixups.
	list_for_each(fixup_child, fixup->children, list_node) {
		DeviceTreeNode *base_child = NULL;
		list_for_each(child, node->children, list_node)
			if (!strcmp(child->name, fixup_child->name)) {
				base_child = child;
				break;
			}

		// All fixup nodes should have a corresponding base node.
		if (!base_child)
			return -1;

		if (dt_fixup_locals(base_child, fixup_child, base) < 0)
			return -1;
	}

	return 0;
}

/*
 * Update all /__symbols__ properties in an overlay that start with
 * "/fragment@X/__overlay__" with corresponding path prefix in the base tree.
 *
 * @param symbols	/__symbols__ done to update.
 * @param fragment	/fragment@X node that references to should be updated.
 * @param base_path	Path of base tree node that the fragment overlaid.
 */
static void dt_fix_symbols(DeviceTreeNode *symbols, DeviceTreeNode *fragment,
			   const char *base_path)
{
	DeviceTreeProperty *prop;
	char buf[512];		// Should be enough for maximum DT path length?
	char node_path[64];	// easily enough for /fragment@XXXX/__overlay__

	if (!symbols)	// If the overlay has no /__symbols__ node, we're done!
		return;

	int len = snprintf(node_path, sizeof(node_path), "/%s/__overlay__",
			   fragment->name);

	list_for_each(prop, symbols->properties, list_node)
		if (!strncmp(prop->prop.data, node_path, len)) {
			prop->prop.size = snprintf(buf, sizeof(buf), "%s%s",
				base_path, (char *)prop->prop.data + len) + 1;
			free(prop->prop.data);
			prop->prop.data = strdup(buf);
		}
}

/*
 * Fix up overlay according to a property in /__fixup__. If the fixed property
 * is a /fragment@X:target, also update /__symbols__ references to fragment.
 *
 * @params overlay	Overlay to fix up.
 * @params fixup	/__fixup__ property.
 * @params phandle	phandle value to insert where the fixup points to.
 * @params base_path	Path to the base DT node that the fixup points to.
 * @params overlay_symbols /__symbols__ node of the overlay.
 *
 * @return		0 on success, -1 on error.
 */
static int dt_fixup_external(DeviceTree *overlay, DeviceTreeProperty *fixup,
			     uint32_t phandle, const char *base_path,
			     DeviceTreeNode *overlay_symbols)
{
	DeviceTreeProperty *prop;

	// External fixup properties are encoded as "<path>:<prop>:<offset>".
	char *entry = fixup->prop.data;
	while ((void *)entry < fixup->prop.data + fixup->prop.size) {
		// okay to destroy fixup property value, won't be needed again
		char *node_path = strsep(&entry, ":");
		char *prop_name = strsep(&entry, ":");
		char *offset_string = entry;
		if (!node_path || !prop_name || !offset_string)
			return -1;

		DeviceTreeNode *ovl_node = dt_find_node_by_path(overlay,
						node_path, NULL, NULL, 0);
		if (!ovl_node)
			return -1;

		DeviceTreeProperty *ovl_prop = NULL;
		list_for_each(prop, ovl_node->properties, list_node)
			if (!strcmp(prop->prop.name, prop_name)) {
				ovl_prop = prop;
				break;
			}

		// Move entry to first char after number, must be a '\0'.
		uint32_t offset = strtoul(offset_string, &entry, 0);
		if (!ovl_prop || offset + 4 > ovl_prop->prop.size || entry[0])
			return -1;
		entry++;  // jump over '\0' to potential next fixup

		be32enc(ovl_prop->prop.data + offset, phandle);

		// If this is a /fragment@X:target property, update
		// references to this fragment in the overlay __symbols__ now.
		if (offset == 0 && !strcmp(prop_name, "target") &&
		    !strchr(node_path + 1, '/')) // only toplevel nodes
			dt_fix_symbols(overlay_symbols, ovl_node, base_path);
	}

	return 0;
}

/*
 * Apply all /__fixup__ properties in the overlay. This will destroy the
 * property data in /__fixup__ and it should not be accessed again.
 *
 * @params tree		Base device tree that the overlay updates.
 * @params symbols	/__symbols__ node of the base device tree.
 * @params overlay	Overlay to fix up.
 * @params fixups	/__fixup__ node in the overlay.
 * @params overlay_symbols /__symbols__ node of the overlay.
 *
 * @return		0 on success, -1 on error.
 */
static int dt_fixup_all_externals(DeviceTree *tree, DeviceTreeNode *symbols,
				  DeviceTree *overlay, DeviceTreeNode *fixups,
				  DeviceTreeNode *overlay_symbols)
{
	DeviceTreeProperty *fix;

	// If we have any external fixups, the base tree must have /__symbols__.
	if (!symbols)
		return -1;

	// Unlike /__local_fixups__, /__fixups__ is not a whole subtree that
	// mirrors the node hierarchy. It's just a directory of fixup properties
	// that each directly contain all information necessary to apply them.
	list_for_each(fix, fixups->properties, list_node) {
		// The name of a fixup property is the label of the node we want
		// a property to phandle-reference. Look it up in /__symbols__.
		const char *path = dt_find_string_prop(symbols, fix->prop.name);
		if (!path)
			return -1;

		// Find the node the label pointed to to figure out its phandle.
		DeviceTreeNode *node = dt_find_node_by_path(tree, path,
							    NULL, NULL, 0);
		if (!node)
			return -1;

		// Write it into the overlay property(s) pointing to that node.
		if (dt_fixup_external(overlay, fix, node->phandle,
				      path, overlay_symbols) < 0)
			return -1;
	}

	return 0;
}

/*
 * Copy all nodes and properties from one DT subtree into another. This is a
 * shallow copy so both trees will point to the same property data afterwards.
 *
 * @params dst		Destination subtree to copy into.
 * @params src		Source subtree to copy from.
 * @params upd		1 to overwrite same-name properties, 0 to discard them.
 */
static void dt_copy_subtree(DeviceTreeNode *dst, DeviceTreeNode *src, int upd)
{
	DeviceTreeProperty *prop;
	DeviceTreeProperty *src_prop;
	list_for_each (src_prop, src->properties, list_node) {
		if (dt_prop_is_phandle(src_prop) ||
		    !strcmp(src_prop->prop.name, "name")) {
			printf("WARNING: ignoring illegal overlay prop '%s'\n",
			       src_prop->prop.name);
			continue;
		}

		DeviceTreeProperty *dst_prop = NULL;
		list_for_each(prop, dst->properties, list_node)
			if (!strcmp(prop->prop.name, src_prop->prop.name)) {
				dst_prop = prop;
				break;
			}

		if (dst_prop) {
			if (!upd) {
				printf("WARNING: ignoring prop update '%s'\n",
				       src_prop->prop.name);
				continue;
			}
		} else {
			dst_prop = alloc_prop();
			list_insert_after(&dst_prop->list_node,
					  &dst->properties);
		}

		dst_prop->prop = src_prop->prop;
	}

	DeviceTreeNode *node;
	DeviceTreeNode *src_node;
	list_for_each (src_node, src->children, list_node) {
		DeviceTreeNode *dst_node = NULL;
		list_for_each (node, dst->children, list_node)
			if (!strcmp(node->name, src_node->name)) {
				dst_node = node;
				break;
			}

		if (!dst_node) {
			dst_node = alloc_node();
			*dst_node = *src_node;
			list_insert_after(&dst_node->list_node, &dst->children);
		} else {
			dt_copy_subtree(dst_node, src_node, upd);
		}
	}
}

/*
 * Apply an overlay /fragment@X node to a base device tree.
 *
 * @param tree		Base device tree.
 * @param fragment	/fragment@X node.
 * @params overlay_symbols /__symbols__ node of the overlay.
 *
 * @return		0 on success, -1 on error.
 */
static int dt_import_fragment(DeviceTree *tree, DeviceTreeNode *fragment,
			      DeviceTreeNode *overlay_symbols)
{
	// The actually overlaid nodes/props are in an __overlay__ child node.
	static const char *overlay_path[] = { "__overlay__", NULL };
	DeviceTreeNode *overlay = dt_find_node(fragment, overlay_path,
					       NULL, NULL, 0);

	// If it doesn't have an __overlay__ child, it's not a fragment.
	if (!overlay)
		return 0;

	// The target node of the fragment can be given by path or by phandle.
	DeviceTreeProperty *prop;
	DeviceTreeProperty *phandle = NULL;
	DeviceTreeProperty *path = NULL;
	list_for_each(prop, fragment->properties, list_node) {
		if (!strcmp(prop->prop.name, "target")) {
			phandle = prop;
			break; // phandle target has priority, stop looking here
		}
		if (!strcmp(prop->prop.name, "target-path"))
			path = prop;
	}

	DeviceTreeNode *target = NULL;
	if (phandle) {
		if (phandle->prop.size != sizeof(uint32_t))
			return -1;
		target = dt_find_node_by_phandle(tree->root,
						 be32dec(phandle->prop.data));
		// Symbols already updated as part of dt_fixup_external(target).
	} else if (path) {
		target = dt_find_node_by_path(tree, path->prop.data,
					      NULL, NULL, 0);
		dt_fix_symbols(overlay_symbols, fragment, path->prop.data);
	}
	if (!target)
		return -1;

	dt_copy_subtree(target, overlay, 1);
	return 0;
}

/*
 * Apply a device tree overlay (flattened) to a base device tree (unflattened).
 * This will destroy/incorporate the overlay blob, so it should not be reused.
 * See dtc.git/Documentation/dt-object-internal.txt for overlay format details.
 *
 * @param tree		Base device tree to add the overlay into.
 * @param overlay_blob	Overlay in raw, unparsed FDT blob form.
 *
 * @return		0 on success, -1 on error.
 */
int dt_apply_overlay(DeviceTree *tree, void *overlay_blob)
{
	DeviceTree *overlay = fdt_unflatten(overlay_blob);
	if (!overlay)
		return -1;

	// First, we need to make sure phandles inside the overlay don't clash
	// with those in the base tree. We just define the highest phandle value
	// in the base tree as the "phandle offset" for this overlay and
	// increment all phandles in it by that value.
	uint32_t phandle_base = tree->max_phandle;
	uint32_t new_max = dt_adjust_all_phandles(overlay->root, phandle_base);
	if (!new_max) {
		printf("ERROR: invalid phandles in overlay\n");
		return -1;
	}
	tree->max_phandle = new_max;

	// Now that we changed phandles in the overlay, we need to update any
	// nodes referring to them. Those are listed in /__local_fixups__.
	DeviceTreeNode *local_fixups = dt_find_node_by_path(overlay,
					"/__local_fixups__", NULL, NULL, 0);
	if (local_fixups && dt_fixup_locals(overlay->root, local_fixups,
					    phandle_base) < 0) {
		printf("ERROR: invalid local fixups in overlay\n");
		return -1;
	}

	// Besides local phandle references (from nodes within the overlay to
	// other nodes within the overlay), the overlay may also contain phandle
	// references to the base tree. These are stored with invalid values and
	// must be updated now. /__symbols__ contains a list of all labels in
	// the base tree, and /__fixups__ describes all nodes in the overlay
	// that contain external phandle references.
	// We also take this opportunity to update all /fragment@X/__overlay__/
	// prefixes in the overlay's /__symbols__ node to the correct path that
	// the fragment will be placed in later, since this is the only step
	// where we have all necessary information for that easily available.
	DeviceTreeNode *symbols = dt_find_node_by_path(tree, "/__symbols__",
						       NULL, NULL, 0);
	DeviceTreeNode *fixups = dt_find_node_by_path(overlay, "/__fixups__",
						      NULL, NULL, 0);
	DeviceTreeNode *overlay_symbols = dt_find_node_by_path(overlay,
						"/__symbols__", NULL, NULL, 0);
	if (fixups && dt_fixup_all_externals(tree, symbols, overlay,
					     fixups, overlay_symbols) < 0) {
		printf("ERROR: cannot match external fixups from overlay\n");
		return -1;
	}

	// After all this fixing up, we can finally merge the overlay into the
	// tree (one fragment at a time, because for some reason it's split up).
	DeviceTreeNode *fragment;
	list_for_each(fragment, overlay->root->children, list_node)
		if (dt_import_fragment(tree, fragment, overlay_symbols) < 0) {
			printf("ERROR: bad DT fragment '%s'\n", fragment->name);
			return -1;
		}

	// We need to also update /__symbols__ to include labels from this
	// overlay, in case we want to load further overlays with external
	// phandle references to it. If the base tree already has a /__symbols__
	// we merge them together, otherwise we just insert the overlay's
	// /__symbols__ node into the base tree root.
	if (overlay_symbols) {
		if (symbols)
			dt_copy_subtree(symbols, overlay_symbols, 0);
		else
			list_insert_after(&overlay_symbols->list_node,
					  &tree->root->children);
	}

	return 0;
}

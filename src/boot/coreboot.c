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
#include <stdlib.h>
#include <string.h>
#include <coreboot_tables.h>
#include <libpayload.h>
#include <sysinfo.h>

#include "base/device_tree.h"
#include "base/init_funcs.h"
#include "config.h"
#include "image/fmap.h"

/* TODO move to common device tree code */
static void bin_prop(ListNode *props, ListNode *old_props,
		     char *name, void *data, int size)
{
	DeviceTreeProperty *prop;

	// Check to see if the kernel already had a property with this name.
	// To avoid unnecessary overhead, only look through properties that
	// were already there under the assumption we won't be trying to
	// overwrite something we just added.
	if (old_props) {
		list_for_each(prop, *old_props, list_node) {
			if (!strcmp(prop->prop.name, name)) {
				prop->prop.data = data;
				prop->prop.size = size;
				return;
			}
		}
	}

	prop = xzalloc(sizeof(*prop));
	list_insert_after(&prop->list_node, props);
	prop->prop.name = name;
	prop->prop.data = data;
	prop->prop.size = size;
}

/* TODO move to common device tree code */
static void string_prop(ListNode *props, ListNode *old_props,
			char *name, char *str)
{
	bin_prop(props, old_props, name, str, strlen(str) + 1);
}

#if 0
/* TODO move to common device tree code (commented out since unused for now). */
static void int_prop(ListNode *props,
		ListNode *old_props, char *name, uint32_t val)
{
	uint32_t *val_ptr = xmalloc(sizeof(val));
	*val_ptr = htobel(val);
	bin_prop(props, old_props, name, val_ptr, sizeof(*val_ptr));
}
#endif

/*
 * Create or update a 'reg' property in a device tree node.
 *
 * @param props		The node's 'properties' list.
 * @param old_props	A pointless parameter (IMHO). TODO: remove
 * @param addrs		Array of address values to be stored in the property.
 * @param sizes		Array of corresponding size values to 'addrs'.
 * @param count		Number of values in 'addrs' and 'sizes' (must be equal).
 * @param addr_cells	Value of #address-cells property valid for this node.
 * @param size_cells	Value of #size-cells property valid for this node.
 */
// TODO move to common device tree code
static void reg_prop(ListNode *props, ListNode *old_props, uintptr_t *addrs,
	size_t *sizes, int count, unsigned addr_cells, unsigned size_cells)
{
	int i;
	size_t length = (addr_cells + size_cells) * sizeof(u32) * count;
	u8 *data = xmalloc(length);
	u8 *cur = data;

	for (i = 0; i < count; i++) {
		if (addr_cells == 2)		// TODO: fix hardcoding
			*(u64 *)cur = htobell(addrs[i]);
		else
			*(u32 *)cur = htobel(addrs[i]);
		cur += addr_cells * sizeof(u32);
		if (size_cells == 2)
			*(u64 *)cur = htobell(sizes[i]);
		else
			*(u32 *)cur = htobel(sizes[i]);
		cur += size_cells * sizeof(u32);
	}

	bin_prop(props, old_props, "reg", data, length);
}

/*
 * Function to find a node from a device tree path, relative to a parent node.
 *
 * @param parent	The node from which to start the relative path lookup.
 * @param path		An array of path component strings that will be looked
 * 			up in order to find the node. Must be terminated with
 * 			a NULL pointer. Example: {'firmware', 'coreboot', NULL}
 * @param addr_cells_ptr	Pointer to an integer that will be updated with
 * 				any #address-cells value found in the path.
 * @param size_cells_ptr	Same as addr_cells_ptr for #size-cells. Must
 * 				either both be valid or both NULL.
 * @param create	1: Create node(s) if not found. 0: Return NULL instead.
 * @return		The found/created node, or NULL.
 */
// TODO move to common device tree code
static DeviceTreeNode *dt_find_node(DeviceTreeNode *parent, const char **path,
		unsigned *addr_cells_ptr, unsigned *size_cells_ptr, int create)
{
	DeviceTreeNode *node, *found = NULL;

	// Update #address-cells and #size-cells for this level if requested.
	if (addr_cells_ptr && size_cells_ptr)
		dt_node_cell_props(parent, addr_cells_ptr, size_cells_ptr);

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

		/*
		 * This data structure will be flattened (= deep copy) before
		 * it is passed to the kernel. Therefore, we can just store a
		 * pointer to the interned string from 'path' here, even though
		 * it lives in depthcharge's .rodata section.
		 */
		found = xzalloc(sizeof(*found));
		found->name = *path;
		list_insert_after(&found->list_node, &parent->children);
	}

	return dt_find_node(found, path + 1, addr_cells_ptr,
			    size_cells_ptr, create);
}

static int install_coreboot_data(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	unsigned addr_cells = 1, size_cells = 1;
	const char *firmware_path[] = { "firmware", NULL };
	DeviceTreeNode *firmware = dt_find_node(tree->root, firmware_path,
						&addr_cells, &size_cells, 1);

	// Need to add 'ranges' to the intermediate node to make 'reg' work.
	bin_prop(&firmware->properties, firmware->properties.next,
		 "ranges", NULL, 0);

	const char *coreboot_path[] = { "coreboot", NULL };
	DeviceTreeNode *coreboot = dt_find_node(firmware, coreboot_path,
						&addr_cells, &size_cells, 1);

	ListNode *props = &coreboot->properties;
	ListNode *old = coreboot->properties.next;

	string_prop(props, old, "compatible", "coreboot");

	struct memrange *cbmem_range = NULL;
	for (int i = lib_sysinfo.n_memranges - 1; i >= 0; i--) {
		if (lib_sysinfo.memrange[i].type == CB_MEM_TABLE) {
			cbmem_range = &lib_sysinfo.memrange[i];
			break;
		}
	}
	if (!cbmem_range)
		return 1;

	uintptr_t reg_addrs[2];
	size_t reg_sizes[2];

	// First 'reg' address range is the coreboot table.
	reg_addrs[0] = (uintptr_t)lib_sysinfo.header;
	reg_sizes[0] = lib_sysinfo.header->header_bytes +
		       lib_sysinfo.header->table_bytes;

	// Second is the CBMEM area (which usually includes the coreboot table).
	reg_addrs[1] = cbmem_range->base;
	reg_sizes[1] = cbmem_range->size;

	reg_prop(props, old, reg_addrs, reg_sizes, 2, addr_cells, size_cells);

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

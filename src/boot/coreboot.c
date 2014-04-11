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

/* TODO move to common device tree code */
static void int_prop(ListNode *props, ListNode *old_props,
		     char *name, uint32_t val)
{
	uint32_t *val_ptr = xmalloc(sizeof(val));
	*val_ptr = htobel(val);
	bin_prop(props, old_props, name, val_ptr, sizeof(*val_ptr));
}

static DeviceTreeNode *dt_find_coreboot_node(DeviceTree *tree)
{
	DeviceTreeNode *node;
	DeviceTreeNode *firmware = NULL;
	DeviceTreeNode *coreboot = NULL;

	// Find the /firmware node, if one exists.
	list_for_each(node, tree->root->children, list_node) {
		if (!strcmp(node->name, "firmware")) {
			firmware = node;
			break;
		}
	}

	// Make one if it didn't.
	if (!firmware) {
		firmware = xzalloc(sizeof(*firmware));
		firmware->name = "firmware";
		list_insert_after(&firmware->list_node, &tree->root->children);
	}

	// Find the /firmware/coreboot node, if one exists
	list_for_each(node, firmware->children, list_node) {
		if (!strcmp(node->name, "coreboot")) {
			coreboot = node;
			break;
		}
	}

	// Make one if it didn't.
	if (!coreboot) {
		coreboot = xzalloc(sizeof(*coreboot));
		coreboot->name = "coreboot";
		list_insert_after(&coreboot->list_node, &firmware->children);
	}

	return coreboot;
}

static int install_coreboot_data(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	DeviceTreeNode *coreboot = dt_find_coreboot_node(tree);

	ListNode *props = &coreboot->properties;
	ListNode *old = coreboot->properties.next;

	string_prop(props, old, "compatible", "coreboot");

	struct memrange *range = NULL;
	for (int i = lib_sysinfo.n_memranges - 1; i >= 0; i--) {
		if (lib_sysinfo.memrange[i].type == CB_MEM_TABLE) {
			range = &lib_sysinfo.memrange[i];
			break;
		}
	}
	if (!range)
		return 1;

	int_prop(props, old, "coreboot-memory", range->base);
	int_prop(props, old, "coreboot-table", (unsigned long)lib_sysinfo.header);

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

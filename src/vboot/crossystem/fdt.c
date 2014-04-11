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
#include <gbb_header.h>
#include <libpayload.h>
#include <stdlib.h>
#include <string.h>
#include <vboot_api.h>
#include <vboot_struct.h>

#include "base/device_tree.h"
#include "config.h"
#include "image/fmap.h"
#include "vboot/crossystem/crossystem.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"

enum {
	VDAT_RW_A = 0,
	VDAT_RW_B = 1,
	VDAT_RECOVERY = 0xFF
};

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

static void string_prop(ListNode *props, ListNode *old_props,
			char *name, char *str)
{
	bin_prop(props, old_props, name, str, strlen(str) + 1);
}

static void int_prop(ListNode *props, ListNode *old_props,
		     char *name, uint32_t val)
{
	uint32_t *val_ptr = xmalloc(sizeof(val));
	*val_ptr = htobel(val);
	bin_prop(props, old_props, name, val_ptr, sizeof(*val_ptr));
}

static DeviceTreeNode *dt_find_chromeos_node(DeviceTree *tree)
{
	DeviceTreeNode *node;
	DeviceTreeNode *firmware = NULL;
	DeviceTreeNode *chromeos = NULL;

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

	// Find the /firmware/chromeos node, if one exists
	list_for_each(node, firmware->children, list_node) {
		if (!strcmp(node->name, "chromeos")) {
			chromeos = node;
			break;
		}
	}

	// Make one if it didn't.
	if (!chromeos) {
		chromeos = xzalloc(sizeof(*chromeos));
		chromeos->name = "chromeos";
		list_insert_after(&chromeos->list_node, &firmware->children);
	}

	return chromeos;
}

static int install_crossystem_data(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	DeviceTreeNode *chromeos = dt_find_chromeos_node(tree);

	ListNode *props = &chromeos->properties;
	ListNode *old = chromeos->properties.next;

	string_prop(props, old, "compatible", "chromeos-firmware");

	void *blob;
	int size;
	if (find_common_params(&blob, &size))
		return 1;
	bin_prop(props, old, "vboot-shared-data", blob, size);
	VbSharedDataHeader *vdat = (VbSharedDataHeader *)blob;

	if (CONFIG_NV_STORAGE_CMOS) {
		string_prop(props, old, "nonvolatile-context-storage", "nvram");
	} else if (CONFIG_NV_STORAGE_CROS_EC) {
		string_prop(props, old, "nonvolatile-context-storage", "mkbp");
	} else if (CONFIG_NV_STORAGE_DISK) {
		string_prop(props, old, "nonvolatile-context-storage", "disk");
		int_prop(props, old, "nonvolatile-context-lba",
			CONFIG_NV_STORAGE_DISK_LBA);
		int_prop(props, old, "nonvolatile-context-offset",
			CONFIG_NV_STORAGE_DISK_OFFSET);
		int_prop(props, old, "nonvolatile-context-size",
			CONFIG_NV_STORAGE_DISK_SIZE);
	}

	int recovery = 0;
	const char *fwid;
	int fwid_size;
	switch (vdat->firmware_index) {
	case VDAT_RW_A:
		fwid = fmap_rwa_fwid();
		fwid_size = fmap_rwa_fwid_size();
		break;
	case VDAT_RW_B:
		fwid = fmap_rwb_fwid();
		fwid_size = fmap_rwb_fwid_size();
		break;
	case VDAT_RECOVERY:
		recovery = 1;
		fwid = fmap_ro_fwid();
		fwid_size = fmap_ro_fwid_size();
		break;
	default:
		printf("Unrecognized firmware index %d.\n",
		       vdat->firmware_index);
		return 1;
	}
	bin_prop(props, old, "firmware-version", (char *)fwid, fwid_size);

	if (recovery)
		string_prop(props, old, "firmware-type", "recovery");
	else if (vdat->flags & VBSD_BOOT_DEV_SWITCH_ON)
		string_prop(props, old, "firmware-type", "developer");
	else
		string_prop(props, old, "firmware-type", "normal");

	int_prop(props, old, "fmap-offset", CONFIG_FMAP_OFFSET);

	bin_prop(props, old, "readonly-firmware-version",
		 (char *)fmap_ro_fwid(), fmap_ro_fwid_size());

	GoogleBinaryBlockHeader *gbb = cparams.gbb_data;
	if (memcmp(gbb->signature, GBB_SIGNATURE, GBB_SIGNATURE_SIZE)) {
		printf("Bad signature on GBB.\n");
		return 1;
	}
	char *hwid = (char *)((uintptr_t)cparams.gbb_data + gbb->hwid_offset);
	bin_prop(props, old, "hardware-id", hwid, gbb->hwid_size);

	if (CONFIG_EC_SOFTWARE_SYNC) {
		int in_rw = 0;

		if (VbExEcRunningRW(&in_rw)) {
			printf("Couldn't tell if the EC firmware is RW.\n");
			return 1;
		}
		string_prop(props, old, "active-ec-firmware",
			    in_rw ? "RW" : "RO");
	}

	return 0;
}

static DeviceTreeFixup crossystem_fixup = {
	.fixup = &install_crossystem_data
};

int crossystem_setup(void)
{
	list_insert_after(&crossystem_fixup.list_node, &device_tree_fixups);
	return 0;
}

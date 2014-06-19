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

static int install_crossystem_data(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	const char *path[] = { "firmware", "chromeos", NULL };
	DeviceTreeNode *node = dt_find_node(tree->root, path, NULL, NULL, 1);

	dt_add_string_prop(node, "compatible", "chromeos-firmware");

	void *blob;
	int size;
	if (find_common_params(&blob, &size))
		return 1;
	dt_add_bin_prop(node, "vboot-shared-data", blob, size);
	VbSharedDataHeader *vdat = (VbSharedDataHeader *)blob;

	if (CONFIG_NV_STORAGE_CMOS) {
		dt_add_string_prop(node, "nonvolatile-context-storage","nvram");
	} else if (CONFIG_NV_STORAGE_CROS_EC) {
		dt_add_string_prop(node, "nonvolatile-context-storage", "mkbp");
	} else if (CONFIG_NV_STORAGE_DISK) {
		dt_add_string_prop(node, "nonvolatile-context-storage", "disk");
		dt_add_u32_prop(node, "nonvolatile-context-lba",
				CONFIG_NV_STORAGE_DISK_LBA);
		dt_add_u32_prop(node, "nonvolatile-context-offset",
				CONFIG_NV_STORAGE_DISK_OFFSET);
		dt_add_u32_prop(node, "nonvolatile-context-size",
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
	dt_add_bin_prop(node, "firmware-version", (char *)fwid, fwid_size);

	if (recovery)
		dt_add_string_prop(node, "firmware-type", "recovery");
	else if (vdat->flags & VBSD_BOOT_DEV_SWITCH_ON)
		dt_add_string_prop(node, "firmware-type", "developer");
	else
		dt_add_string_prop(node, "firmware-type", "normal");

	dt_add_u32_prop(node, "fmap-offset", CONFIG_FMAP_OFFSET);

	dt_add_bin_prop(node, "readonly-firmware-version",
		 (char *)fmap_ro_fwid(), fmap_ro_fwid_size());

	GoogleBinaryBlockHeader *gbb = cparams.gbb_data;
	if (memcmp(gbb->signature, GBB_SIGNATURE, GBB_SIGNATURE_SIZE)) {
		printf("Bad signature on GBB.\n");
		return 1;
	}
	char *hwid = (char *)((uintptr_t)cparams.gbb_data + gbb->hwid_offset);
	dt_add_bin_prop(node, "hardware-id", hwid, gbb->hwid_size);

	if (CONFIG_EC_SOFTWARE_SYNC) {
		int in_rw = 0;

		if (VbExEcRunningRW(&in_rw)) {
			printf("Couldn't tell if the EC firmware is RW.\n");
			return 1;
		}
		dt_add_string_prop(node, "active-ec-firmware",
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

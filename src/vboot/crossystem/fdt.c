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
#include "vboot/callbacks/nvstorage_flash.h"
#include "vboot/crossystem/crossystem.h"
#include "vboot/firmware_id.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"

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
		dt_add_string_prop(node,
				"nonvolatile-context-storage", "cros-ec");
	} else if (CONFIG_NV_STORAGE_DISK) {
		dt_add_string_prop(node, "nonvolatile-context-storage", "disk");
		dt_add_u32_prop(node, "nonvolatile-context-lba",
				CONFIG_NV_STORAGE_DISK_LBA);
		dt_add_u32_prop(node, "nonvolatile-context-offset",
				CONFIG_NV_STORAGE_DISK_OFFSET);
		dt_add_u32_prop(node, "nonvolatile-context-size",
				CONFIG_NV_STORAGE_DISK_SIZE);
	} else if (CONFIG_NV_STORAGE_FLASH) {
		dt_add_string_prop(node, "nonvolatile-context-storage","flash");
		dt_add_u32_prop(node, "nonvolatile-context-offset",
				nvstorage_flash_get_offet());
		dt_add_u32_prop(node, "nonvolatile-context-size",
				nvstorage_flash_get_blob_size());
	}

	int fw_index = vdat->firmware_index;
	const char *fwid;
	int fwid_size;

	fwid = get_fw_id(fw_index);

	if (fwid == NULL) {
		printf("Unrecognized firmware index %d.\n", fw_index);
		return 1;
	}

	fwid_size = get_fw_size(fw_index);

	dt_add_bin_prop(node, "firmware-version", (char *)fwid, fwid_size);

	if (fw_index == VDAT_RECOVERY)
		dt_add_string_prop(node, "firmware-type", "recovery");
	else if (vdat->flags & VBSD_BOOT_DEV_SWITCH_ON)
		dt_add_string_prop(node, "firmware-type", "developer");
	else
		dt_add_string_prop(node, "firmware-type", "normal");

	dt_add_u32_prop(node, "fmap-offset", CONFIG_FMAP_OFFSET);

	int ro_fw_size = get_ro_fw_size();

	if (ro_fw_size)
		dt_add_bin_prop(node, "readonly-firmware-version",
				(char *)get_ro_fw_id(), ro_fw_size);

	GoogleBinaryBlockHeader *gbb = cparams.gbb_data;
	if (memcmp(gbb->signature, GBB_SIGNATURE, GBB_SIGNATURE_SIZE)) {
		printf("Bad signature on GBB.\n");
		return 1;
	}
	char *hwid = (char *)((uintptr_t)cparams.gbb_data + gbb->hwid_offset);
	dt_add_bin_prop(node, "hardware-id", hwid, gbb->hwid_size);

	if (CONFIG_EC_SOFTWARE_SYNC) {
		int in_rw = 0;

		if (VbExEcRunningRW(0, &in_rw)) {
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

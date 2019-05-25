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
 */

#include <assert.h>
#include <endian.h>
#include <libpayload.h>
#include <stdlib.h>
#include <string.h>
#include <vboot_api.h>
#include <vboot_struct.h>

#include "base/device_tree.h"
#include "image/fmap.h"
#include "vboot/callbacks/nvstorage_flash.h"
#include "vboot/crossystem/crossystem.h"
#include "vboot/firmware_id.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"

static int last_firmware_type = FIRMWARE_TYPE_AUTO_DETECT;

static int install_crossystem_data(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	const char *path[] = { "firmware", "chromeos", NULL };
	DeviceTreeNode *node = dt_find_node(tree->root, path, NULL, NULL, 1);

	dt_add_string_prop(node, "compatible", "chromeos-firmware");

	VbSharedDataHeader *vb_sd;
	int size;

	if (find_common_params((void**)&vb_sd, &size) != 0) {
		printf("Can't find common params.\n");
		return 1;
	}
	dt_add_bin_prop(node, "vboot-shared-data", vb_sd, size);

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

	int fw_index = vb_sd->firmware_index;
	const char *fwid;
	int fwid_size;

	fwid = get_fw_id(fw_index);

	if (fwid == NULL) {
		printf("Unrecognized firmware index %d.\n", fw_index);
		return 1;
	}

	fwid_size = get_fw_size(fw_index);

	dt_add_bin_prop(node, "firmware-version", (char *)fwid, fwid_size);
	char *type_names[] = {
		[FIRMWARE_TYPE_RECOVERY] = "recovery",
		[FIRMWARE_TYPE_NORMAL] = "normal",
		[FIRMWARE_TYPE_DEVELOPER] = "developer",
		[FIRMWARE_TYPE_NETBOOT] = "netboot",
		[FIRMWARE_TYPE_LEGACY] = "legacy",
	};

	int fw_type = last_firmware_type;
	if (fw_type == FIRMWARE_TYPE_AUTO_DETECT) {
		if (fw_index == VBSD_RECOVERY)
			fw_type = FIRMWARE_TYPE_RECOVERY;
		else if (vb_sd->flags & VBSD_BOOT_DEV_SWITCH_ON)
			fw_type = FIRMWARE_TYPE_DEVELOPER;
		else
			fw_type = FIRMWARE_TYPE_NORMAL;
	}
	if (fw_type < 0 || fw_type >= ARRAY_SIZE(type_names))
		dt_add_string_prop(node, "firmware-type", "unknown");
	else
		dt_add_string_prop(node, "firmware-type", type_names[fw_type]);

	dt_add_u32_prop(node, "fmap-offset", lib_sysinfo.fmap_offset);

	int ro_fw_size = get_ro_fw_size();

	if (ro_fw_size)
		dt_add_bin_prop(node, "readonly-firmware-version",
				(char *)get_ro_fw_id(), ro_fw_size);

	char *hwid;
	uint32_t hwid_size;
	gbb_get_hwid(&hwid, &hwid_size);
	dt_add_bin_prop(node, "hardware-id", hwid, hwid_size);

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

int crossystem_setup(int firmware_type)
{
	// Never run crossystem_setup() twice or you'll get a circle in the
	// fixup list that leads to an infinite loop when working through them!
	assert(crossystem_fixup.list_node.next == NULL);

	/* TODO(hungte): To do this more gracefully, we should consider adding a
	 * firmware type field to vdat or a new flag to indicate legacy boot,
	 * depending on if we find that really being very useful.
	 */
	last_firmware_type = firmware_type;
	list_insert_after(&crossystem_fixup.list_node, &device_tree_fixups);
	return 0;
}

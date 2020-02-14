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
#include <vb2_api.h>

#include "base/device_tree.h"
#include "image/fmap.h"
#include "vboot/crossystem/crossystem.h"
#include "vboot/firmware_id.h"
#include "vboot/nvdata/flash.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"

static int last_firmware_type = FIRMWARE_TYPE_AUTO_DETECT;

static int install_crossystem_data(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	struct vb2_context *ctx = vboot_get_context();
	const char *path[] = { "firmware", "chromeos", NULL };
	DeviceTreeNode *node = dt_find_node(tree->root, path, NULL, NULL, 1);

	/* Never free vbsd, so it sticks around until dt_flatten() is called. */
	uint8_t *vbsd = malloc(VB2_VBSD_SIZE);
	vb2api_export_vbsd(ctx, flag_fetch(FLAG_WPSW), vbsd);
	dt_add_bin_prop(node, "vboot-shared-data", vbsd, VB2_VBSD_SIZE);

	dt_add_string_prop(node, "compatible", "chromeos-firmware");

	if (CONFIG_NVDATA_CMOS) {
		dt_add_string_prop(node, "nonvolatile-context-storage",
				   "nvram");
	} else if (CONFIG_NVDATA_CROS_EC) {
		dt_add_string_prop(node, "nonvolatile-context-storage",
				   "cros-ec");
	} else if (CONFIG_NVDATA_FLASH) {
		dt_add_string_prop(node, "nonvolatile-context-storage",
				   "flash");
		dt_add_u32_prop(node, "nonvolatile-context-offset",
				nvdata_flash_get_offset());
		dt_add_u32_prop(node, "nonvolatile-context-size",
				nvdata_flash_get_blob_size());
	}

	const char *fwid;
	int fwid_size;

	fwid = get_active_fw_id();
	fwid_size = get_active_fw_size();
	if (fwid == NULL || fwid_size == 0) {
		printf("Unrecognized active firmware index.\n");
		return 1;
	}


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
		if (ctx->flags & VB2_CONTEXT_RECOVERY_MODE)
			fw_type = FIRMWARE_TYPE_RECOVERY;
		if (ctx->flags & VB2_CONTEXT_DEVELOPER_MODE)
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

	static char hwid[VB2_GBB_HWID_MAX_SIZE];
	uint32_t hwid_size = sizeof(hwid);
	if (!vb2api_gbb_read_hwid(vboot_get_context(), hwid, &hwid_size))
		dt_add_bin_prop(node, "hardware-id", hwid, hwid_size);

	if (CONFIG_EC_SOFTWARE_SYNC) {
		int in_rw = 0;

		if (vb2ex_ec_running_rw(&in_rw)) {
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

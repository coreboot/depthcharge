/*
 * Copyright 2015 Google Inc.
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

#include <libpayload.h>

#include "base/device_tree.h"
#include "base/init_funcs.h"
#include "boot/android_dt.h"
#include "vboot/stages.h"

static const char *get_verifiedbootstate(void)
{
	if (vboot_in_developer())
		return "orange";
	else
		return "green";
}

static int fix_device_tree(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	const char *firmware_dt_name[] = { "firmware", NULL };
	const char *android_dt_name[] = { "android", NULL };
	DeviceTreeNode *firmware_node;
	DeviceTreeNode *android_node;
	int i;
	const struct android_dt_prop {
		const char *name;
		const char *value;
	} maps[] = {
		{ "compatible", "android,firmware" },
		{ "hardware", hardware_name() },
		{ "serialno", lib_sysinfo.serialno },
		{ "verifiedbootstate", get_verifiedbootstate() },
	};

	firmware_node = dt_find_node(tree->root, firmware_dt_name,
				     NULL, NULL, 1);
	if (!firmware_node) {
		printf("failed to create node /%s\n", firmware_dt_name[0]);
		return 1;
	}

	android_node = dt_find_node(firmware_node, android_dt_name,
				    NULL, NULL, 1);
	if (!android_node) {
		printf("failed to create node /%s/%s\n",
		       firmware_dt_name[0], android_dt_name[0]);
		return 1;
	}

	for (i = 0; i < ARRAY_SIZE(maps); i++) {
		/*
		 * Some attributes might be NULL if hardware_name isn't
		 * implemented, or if the field in the VPD isn't set.
		 */
		if (maps[i].value)
			dt_add_string_prop(android_node,
					   (char *)maps[i].name,
					   (char *)maps[i].value);
	}
	return 0;
}

static DeviceTreeFixup android_dt_fixup = {
	.fixup = fix_device_tree
};

static int android_dt_setup(void)
{
	list_insert_after(&android_dt_fixup.list_node, &device_tree_fixups);
	return 0;
}

INIT_FUNC(android_dt_setup);

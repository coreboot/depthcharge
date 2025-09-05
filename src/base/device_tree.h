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

#ifndef __BASE_DEVICE_TREE_H__
#define __BASE_DEVICE_TREE_H__

#include <commonlib/device_tree.h>
#include <commonlib/list.h>
#include <stdint.h>

/*
 * Structure defining mapping between a VPD field and the device tree
 * path to the property corresponding to the field.
 */
typedef struct {
	const char *vpd_name;
	const char *dt_path;
} VpdDeviceTreeMap;

typedef struct
{
	struct device_tree_fixup fixup;
	const VpdDeviceTreeMap *map;
} VpdDeviceTreeFixup;

void dt_register_vpd_mac_fixup(const VpdDeviceTreeMap *map);

/*
 * Structure defining mapping between arbitrary objects and the device tree
 * path to the property corresponding to the object.
 */
typedef struct {
	int force_create; /* If false - do not create a new node. */
	const char *dt_path;
	const char *key;
} DtPathMap;

/*
 * DEPRECATED: use dt_register_vpd_mac_fixup() instead
 *
 * Copy mac addresses from sysinfo table into the device tree. The mapping
 * between the dt_maps entries and sysinfo mac address table elements is
 * implicit, i.e. the device tree node found in the maps entry, gets assinged
 * the mac address found in the sysinfo table, in the same order.
 */
int dt_set_mac_addresses(struct device_tree *tree, const DtPathMap *dt_maps);

/*
 * Copy WIFI calibration data from sysinfo table into the device tree. Each
 * WIFI calibration blob stored the sysinfo table contains key and data. The
 * key is used for mapping into the device tree path. The data becomes the
 * contents of the device tree property at that path.
 */
int dt_set_wifi_calibration(struct device_tree *tree, const DtPathMap *maps);

/*
 * Retrieve Country Code data from VPD and add it into the device tree.
 */
int dt_set_wifi_country_code(struct device_tree *tree, const DtPathMap *maps);

/*
 * Retrieve Xo-cal-data from VPD and add it to the device tree.
 */
int dt_set_xo_cal_data(struct device_tree *tree, const DtPathMap *maps);

#endif /* __BASE_DEVICE_TREE_H__ */

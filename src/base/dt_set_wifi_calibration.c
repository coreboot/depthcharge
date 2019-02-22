/*
 * Copyright 2014 Google Inc.
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
#include <string.h>
#include "base/device_tree.h"
#include "base/vpd_util.h"

/*
 * This file provides functions retrieving WiFi calibration data from CBMEM
 * and placing it in the kernel device tree.
 *
 * Per interface calibration data is stored in a CBMEM entry as a header
 * followed by a few adjacent blobs, as described by the following structures.
 */

/* A single calibration data blob */
struct calibration_blob {
	uint32_t blob_size;  /* Total size. rounded up to fall on a 4 byte
				   boundary. */
	uint32_t key_size;   /* Size of the name of this entry, \0 included. */
	uint32_t value_size; /* Size of the value of this entry */
	/* Zero terminated name(key) goes here, immediately followed by value */
};

/*
 * This is the structure of the CBMEM entry containing WiFi calibration blobs.
 * It starts with the total size (header size included) followed by an
 * arbitrary number of concatenated 4 byte aligned calibration blobs.
 */
struct calibration_entry {
	uint32_t size;
	struct calibration_blob entries[0];  /* A varialble size container. */
};

static int blob_is_valid(struct calibration_entry *cal_entry,
			 struct calibration_blob *cal_blob)
{
	uintptr_t entry_base, blob_base;
	uintptr_t blob_size;

	entry_base = (uintptr_t)cal_entry;
	blob_base = (uintptr_t)cal_blob;

	if ((blob_base <= entry_base) ||
	    (blob_base > (entry_base + cal_entry->size)) ||
	    (blob_base & 3))
		return 0;

	blob_size = sizeof(struct calibration_blob) +
		cal_blob->key_size +
		cal_blob->value_size;

	if (cal_blob->blob_size < blob_size)
		return 0;

	if ((blob_base + blob_size) > (entry_base + cal_entry->size))
		return 0;

	return 1;
}

int dt_set_wifi_calibration(DeviceTree *tree, const DtPathMap *maps)
{
	int rv = 0;
	struct calibration_entry *cal_entry;
	struct calibration_blob *cal_blob;

	cal_entry = lib_sysinfo.wifi_calibration;

	if (!cal_entry)
		return 0;	/* No calibration data was found. */

	/* The first blob starts right above the header */
	cal_blob = cal_entry->entries;

	while(blob_is_valid(cal_entry, cal_blob)) {
		const DtPathMap *map = maps;

		while (map->dt_path) {
			char *key = (char *)(cal_blob + 1);

			if (!strcmp(map->key, key)) {
				rv |= dt_set_bin_prop_by_path
					(tree, map->dt_path,
					 key + cal_blob->key_size,
					 cal_blob->value_size,
					 map->force_create);
				break;
			}

			map++;
		}

		if (!map->dt_path)
			printf("%s: did not find mapping for %s\n",
			       __func__, map->key);

		cal_blob = (struct calibration_blob *)
			((uintptr_t)cal_blob + cal_blob->blob_size);
	}

	return rv;
}

int dt_set_wifi_country_code(DeviceTree *tree, const DtPathMap *maps)
{
	int rv = 0;
	DeviceTreeNode *dt_node;
	const DtPathMap *map = maps;
	const char regioncode_key[] = "region";
	char country_code[8], *cc;

	if (!vpd_gets(regioncode_key, country_code, sizeof(country_code))) {
		printf("No region code found in VPD\n");
		return rv;
	}

	/* Add only the two letter Alpha-2 country code, remove the variant
	 * in region code.
	 */
	country_code[2] = 0;
	cc = strdup(country_code);

	for (; map && map->dt_path; map++) {
		dt_node = dt_find_node_by_path(tree, map->dt_path, NULL,
						NULL, map->force_create);
		if (!dt_node) {
			rv |= 1;
			continue;
		}
		dt_add_string_prop(dt_node, (char *)map->key, cc);
	}

	return rv;
}

int dt_set_xo_cal_data(DeviceTree *tree, const DtPathMap *maps)
{
	int key_val, rv = 0;
	DeviceTreeNode *dt_node;
	const DtPathMap *map = maps;
	const char key[] = "xo_cal_data";
	char key_value[12];

	if (!vpd_gets(key, key_value, sizeof(key_value))) {
		printf("No %s found in VPD\n",key);
		return rv;
	}

	/* Convert the returned string to integer */
	key_val = strtol(key_value, NULL, 10);

	for (; map && map->dt_path; map++) {
		dt_node = dt_find_node_by_path(tree, map->dt_path, NULL,
				NULL, map->force_create);
		if (!dt_node) {
			rv |= 1;
			continue;
		}
		dt_add_u32_prop(dt_node, (char *)map->key, key_val);
	}

	return rv;
}

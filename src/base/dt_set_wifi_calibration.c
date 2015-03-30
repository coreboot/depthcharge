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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>
#include "base/device_tree.h"

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

/* Mapping of interface numbers into the wifi device address on the PCI bus. */
static const uint32_t if_to_address[] = { 0x1b500000, 0x1b700000, 0x1b900000 };

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

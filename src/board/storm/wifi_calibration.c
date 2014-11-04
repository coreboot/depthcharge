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

#include "board.h"

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

/*
 * The keys' values match one of two templates followed by a single digit, the
 * interface number.
 */
static const char * const templates[] = {
	"wifi_base64_calibration",
	"wifi_calibration"
};

/*
 * The string below represents the location of the blobs in the device tree
 * passed to the kernel. The %d is replaced by a number derived from the blob
 * index. In case the calibration data is stored in base64 form (as defined by
 * the key), the last node name needs to be extended by "-base64".
 */
static const char *dt_path = "soc/pci@%8.8x/pcie@0/ath10k@0,0";

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

static void process_blob(DeviceTree *tree,
			 struct calibration_blob *cal_blob,
			 const char *templ,
			 int base64)
{
	void *value;
	char *key = (char *)(cal_blob + 1);
	int templ_len = strlen(templ);
	int if_index;
	int string_size;
	char path_str[100];  /* Should be enough for a longest path. */
	DeviceTreeNode *node;

	if (((templ_len + 2) != cal_blob->key_size) ||
	    strncmp(key, templ, templ_len))
		return;		/* Key does not match. */

	if_index = key[templ_len] - '0';

	if ((if_index < 0) || (if_index >= ARRAY_SIZE(if_to_address)))
		return;

	string_size = snprintf(path_str, sizeof(path_str),
			       dt_path, if_to_address[if_index]);
	if (string_size >= sizeof(path_str))
		return;

	/*
	 * Binary nodes are supposed to be present in the device tree, the
	 * base64 nodes need to be created.
	 */
	node = dt_find_node_by_path(tree->root, path_str, NULL, NULL, base64);
	if (!node)
		return;

	/* The value data starts right above the key. */
	value = key + cal_blob->key_size;
	dt_add_bin_prop(node, base64 ?
			"qcom,ath10k-calibration-data-base64" :
			"qcom,ath10k-calibration-data",
			value, cal_blob->value_size);
}

int set_wifi_calibration(DeviceTree *tree)
{
	int i;
	struct calibration_entry *cal_entry;
	struct calibration_blob *cal_blob;

	cal_entry = lib_sysinfo.wifi_calibration;

	if (!cal_entry)
		return 0;	/* No calibration data was found. */

	/* The first blob starts right above the header */
	cal_blob = cal_entry->entries;

	while(blob_is_valid(cal_entry, cal_blob)) {

		for (i = 0; i < ARRAY_SIZE(templates); i++)
			process_blob(tree, cal_blob, templates[i],
				     strstr(templates[i], "_base64") != NULL);

		cal_blob = (struct calibration_blob *)
			((uintptr_t)cal_blob + cal_blob->blob_size);
	}

	return 0;
}


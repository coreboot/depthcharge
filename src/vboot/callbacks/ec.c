/*
 * Copyright 2012 Google Inc.
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
#include <libpayload.h>
#include <cbfs.h>
#include <vboot_api.h>

#include <stddef.h>
#include <vb2_api.h>

#include "base/timestamp.h"
#include "config.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/cbfs.h"
#include "image/fmap.h"
#include "image/index.h"
#include "vboot/util/flag.h"


static struct cbfs_media *ro_cbfs;
/* Returns the CBFS filename for EC firmware images based on the device index
 * passed in from vboot.
 */
static const char *get_fw_filename(int devidx, int select)
{
	if (select == VB_SELECT_FIRMWARE_READONLY)
		return "ecro";
	return devidx == 0 ? "ecrw" : "pdrw";
}

static struct cbfs_handle *find_file_in_cbfs(
	const char *filename, enum VbSelectFirmware_t select)
{
	if (!IS_ENABLED(CONFIG_DRIVER_CBFS_FLASH))
		return NULL;

	if (select == VB_SELECT_FIRMWARE_READONLY) {
		printf("Trying to locate '%s' in RO CBFS\n", filename);
		if (ro_cbfs == NULL)
			ro_cbfs = cbfs_ro_media();
		return cbfs_get_handle(ro_cbfs, filename);
	}

	printf("Trying to locate '%s' in CBFS\n", filename);
	return cbfs_get_handle(CBFS_DEFAULT_MEDIA, filename);
}

static const uint8_t *get_file_hash_from_cbfs(
	const char *filename, int *hash_size, enum VbSelectFirmware_t select)
{
	printf("Trying to fetch hash for '%s'\n", filename);
	struct cbfs_handle *handle = find_file_in_cbfs(filename, select);

	if (handle == NULL) {
		printf("%s not found\n", filename);
		return NULL;
	}

	struct cbfs_file_attr_hash *attr = cbfs_get_attr(handle,
		CBFS_FILE_ATTR_TAG_HASH);

	if (attr == NULL) {
		printf("%s found, but without hash\n", filename);
		return NULL;
	}

	if (ntohl(attr->hash_type) != VB2_HASH_SHA256) {
		printf("hash is not SHA256\n");
		return NULL;
	}
	*hash_size = ntohl(attr->len) - sizeof(*attr);
	return attr->hash_data;
}

int VbExTrustEC(int devidx)
{
	int val;

	if (devidx != 0)
		return 0;

	val = flag_fetch(FLAG_ECINRW);
	if (val < 0) {
		printf("Couldn't tell if the EC is running RW firmware.\n");
		return 0;
	}
	// Trust the EC if it's NOT in its RW firmware.
	return !val;
}

VbError_t VbExEcRunningRW(int devidx, int *in_rw)
{
	VbootEcOps *ec = vboot_ec[devidx];
	assert(ec && ec->running_rw);
	return ec->running_rw(ec, in_rw);
}

VbError_t VbExEcJumpToRW(int devidx)
{
	VbootEcOps *ec = vboot_ec[devidx];
	assert(ec && ec->jump_to_rw);
	return ec->jump_to_rw(ec);
}

VbError_t VbExEcDisableJump(int devidx)
{
	VbootEcOps *ec = vboot_ec[devidx];
	assert(ec && ec->disable_jump);
	return ec->disable_jump(ec);
}

VbError_t VbExEcHashImage(int devidx, enum VbSelectFirmware_t select,
			  const uint8_t **hash, int *hash_size)
{
	VbootEcOps *ec = vboot_ec[devidx];
	assert(ec && ec->hash_image);
	return ec->hash_image(ec, select, hash, hash_size);
}

VbError_t VbExEcGetExpectedImage(int devidx, enum VbSelectFirmware_t select,
				 const uint8_t **image, int *image_size)
{
	const char *name;

	switch (select) {
	case VB_SELECT_FIRMWARE_A:
		name = (devidx == 0 ? "EC_MAIN_A" : "PD_MAIN_A");
		break;
	case VB_SELECT_FIRMWARE_B:
		name = (devidx == 0 ? "EC_MAIN_B" : "PD_MAIN_B");
		break;
	case VB_SELECT_FIRMWARE_READONLY:
		name = "FW_MAIN_RO";
		break;
	default:
		printf("Unrecognized EC firmware requested.\n");
		return VBERROR_UNKNOWN;
	}

	FmapArea area;
	if (fmap_find_area(name, &area)) {
		printf("Didn't find section %s in the fmap.\n", name);
		/* It may be gone in favor of CBFS based RW sections,
		 * so look there, too. */
		size_t size;
		const char *filename = get_fw_filename(devidx, select);
		struct cbfs_handle *h = find_file_in_cbfs(filename, select);
		if (h == NULL)
			return VBERROR_UNKNOWN;
		*image = cbfs_get_contents(h, &size, 0);
		free(h);
		if (*image == NULL)
			return VBERROR_UNKNOWN;
		*image_size = size;
		printf("found '%s', sized 0x%zx\n", filename, size);
		return VBERROR_SUCCESS;
	}

	uint32_t size;
	*image = index_subsection(&area, 0, &size);
	*image_size = size;
	if (!*image)
		return VBERROR_UNKNOWN;

	printf("EC-%s firmware address, size are %p, %d.\n", select ==
	       VB_SELECT_FIRMWARE_READONLY ? "RO" : "RW", *image, *image_size);

	return VBERROR_SUCCESS;
}

VbError_t VbExEcGetExpectedImageHash(int devidx, enum VbSelectFirmware_t select,
				     const uint8_t **hash, int *hash_size)
{
	const char *name;

	switch (select) {
	case VB_SELECT_FIRMWARE_A:
		name = "FW_MAIN_A";
		break;
	case VB_SELECT_FIRMWARE_B:
		name = "FW_MAIN_B";
		break;
	case VB_SELECT_FIRMWARE_READONLY:
		name = "FW_MAIN_RO";
		break;
	default:
		printf("Unrecognized EC hash requested.\n");
		return VBERROR_UNKNOWN;
	}

	FmapArea area;
	if (fmap_find_area(name, &area))
		printf("Didn't find section %s in the fmap.\n", name);

	uint32_t size;

	/*
	 * Assume the hash is subsection (devidx+1) in the main firmware.  This
	 * is currently true (see for example, board/samus/fmap.dts), but
	 * fragile as all heck.
	 *
	 * TODO(rspangler@chromium.org): More robust way of locating the hash.
	 * Subsections should really have names/tags, not just indices.
	 */
	*hash = index_subsection(&area, devidx + 1, &size);
	*hash_size = size;

	if (!*hash) {
		printf("Didn't find precalculated hash subsection %d.\n",
		       devidx + 1);
		const char *filename = get_fw_filename(devidx, select);
		*hash = get_file_hash_from_cbfs(filename, hash_size, select);
		if (!*hash)
			return VBERROR_UNKNOWN;
	}

	printf("EC-%s hash address, size are %p, %d.\n", select ==
	       VB_SELECT_FIRMWARE_READONLY ? "RO" : "RW", *hash, *hash_size);

	printf("Hash = ");
	for (int i = 0; i < *hash_size; i++)
		printf("%02x", (*hash)[i]);
	printf("\n");

	return VBERROR_SUCCESS;
}

VbError_t VbExEcUpdateImage(int devidx, enum VbSelectFirmware_t select,
			    const uint8_t *image, int image_size)
{
	VbootEcOps *ec = vboot_ec[devidx];
	assert(ec && ec->update_image);
	return ec->update_image(ec, select, image, image_size);
}

VbError_t VbExEcProtect(int devidx, enum VbSelectFirmware_t select)
{
	VbootEcOps *ec = vboot_ec[devidx];
	assert(ec && ec->protect);
	return ec->protect(ec, select);
}

VbError_t VbExEcEnteringMode(int devidx, enum VbEcBootMode_t mode)
{
	VbootEcOps *ec = vboot_ec[devidx];
	assert(ec && ec->entering_mode);
	return ec->entering_mode(ec, mode);
}

/* Wait 3 seconds after software sync for EC to clear the limit power flag. */
#define LIMIT_POWER_WAIT_TIMEOUT 3000
/* Check the limit power flag every 50 ms while waiting. */
#define LIMIT_POWER_POLL_SLEEP 50

VbError_t VbExEcVbootDone(int in_recovery)
{
	int limit_power;
	int limit_power_wait_time = 0;
	int message_printed = 0;

	/* Ensure we have enough power to continue booting */
	while(1) {
		if (cros_ec_read_limit_power_request(&limit_power)) {
			printf("Failed to check EC limit power flag.\n");
			return VBERROR_UNKNOWN;
		}

		/*
		 * Do not wait for the limit power flag to be cleared in
		 * recovery mode since we didn't just sysjump.
		 */
		if (!limit_power || in_recovery ||
		    limit_power_wait_time > LIMIT_POWER_WAIT_TIMEOUT)
			break;

		if (!message_printed) {
			printf("Waiting for EC to clear limit power flag.\n");
			message_printed = 1;
		}

		mdelay(LIMIT_POWER_POLL_SLEEP);
		limit_power_wait_time += LIMIT_POWER_POLL_SLEEP;
	}

	if (limit_power) {
		printf("EC requests limited power usage. Request shutdown.\n");
		return VBERROR_SHUTDOWN_REQUESTED;
	}

	timestamp_add_now(TS_VB_EC_VBOOT_DONE);
	return VBERROR_SUCCESS;
}

VbError_t VbExEcBatteryCutOff(void) {
	 return (cros_ec_battery_cutoff(0) == 0 ? VBERROR_SUCCESS :
		 VBERROR_UNKNOWN);
}

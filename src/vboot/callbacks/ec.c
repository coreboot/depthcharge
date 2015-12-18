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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
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

/* Returns the CBFS filename for EC firmware images based on the device index
 * passed in from vboot.
 */
static const char *get_fw_filename(int devidx, int select)
{
	if (select == VB_SELECT_FIRMWARE_READONLY)
		return "ecro";
	return devidx == 0 ? "ecrw" : "pdrw";
}

static enum ec_flash_region get_flash_region(enum VbSelectFirmware_t select)
{
	return (select == VB_SELECT_FIRMWARE_READONLY) ? EC_FLASH_REGION_WP_RO :
		EC_FLASH_REGION_RW;
}

static struct cbfs_file *get_file_from_cbfs(const char *fmap_name,
	const char *filename)
{
	struct cbfs_media media;
	struct cbfs_file *file;

	if (!IS_ENABLED(CONFIG_DRIVER_CBFS_FLASH))
		return NULL;

	if (cbfs_media_from_fmap(&media, fmap_name)) {
		printf("couldn't access cbfs media\n");
		return NULL;
	}

	file = cbfs_get_file(&media, filename);
	media.close(&media);

	return file;
}

static const uint8_t *get_file_hash_from_cbfs(const char *fmap_name,
	const char *filename, int *hash_size)
{
	printf("Trying to fetch hash for '%s' from CBFS\n", filename);
	struct cbfs_file *file = get_file_from_cbfs(fmap_name, filename);

	if (file == NULL) {
		printf("file not found\n");
		return NULL;
	}

	struct cbfs_file_attribute *attr = cbfs_file_find_attr(file,
		CBFS_FILE_ATTR_TAG_HASH);

	if (attr == NULL) {
		printf("ECRW found, but without hash\n");
		return NULL;
	}

	struct cbfs_file_attr_hash *hash = (struct cbfs_file_attr_hash *)attr;

	if (ntohl(hash->hash_type) != VB2_HASH_SHA256) {
		printf("hash is not SHA256\n");
		return NULL;
	}
	*hash_size = ntohl(hash->len) - sizeof(*hash);
	return hash->hash_data;
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
	enum ec_current_image image;

	if (cros_ec_read_current_image(devidx, &image) < 0) {
		printf("Failed to read current EC image.\n");
		return VBERROR_UNKNOWN;
	}
	switch (image) {
	case EC_IMAGE_RO:
		*in_rw = 0;
		break;
	case EC_IMAGE_RW:
		*in_rw = 1;
		break;
	default:
		printf("Unrecognized EC image type %d.\n", image);
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

VbError_t VbExEcJumpToRW(int devidx)
{
	if (cros_ec_reboot(devidx, EC_REBOOT_JUMP_RW, 0) < 0) {
		printf("Failed to make the EC jump to RW.\n");
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

VbError_t VbExEcDisableJump(int devidx)
{
	if (cros_ec_reboot(devidx, EC_REBOOT_DISABLE_JUMP, 0) < 0) {
		printf("Failed to make the EC disable jumping.\n");
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

VbError_t VbExEcHashImage(int devidx, enum VbSelectFirmware_t select,
			  const uint8_t **hash, int *hash_size)
{
	static struct ec_response_vboot_hash resp;

	if (cros_ec_read_hash(devidx, get_flash_region(select), &resp) < 0) {
		printf("Failed to read EC hash.\n");
		return VBERROR_UNKNOWN;
	}

	/*
	 * TODO (rspangler@chromium.org): the code below isn't very tolerant
	 * of errors.
	 *
	 * If the EC is busy calculating a hash, we should wait and retry
	 * reading the hash status.
	 *
	 * If the hash is unavailable, the wrong type, or covers the wrong
	 * offset/size (which we need to get from the FDT, since it's
	 * board-specific), we should request a new hash and wait for it to
	 * finish.  Also need a flag to force it to rehash, which we'll use
	 * after doing a firmware update.
	 */
	if (resp.status != EC_VBOOT_HASH_STATUS_DONE) {
		printf("EC hash wasn't finished.\n");
		return VBERROR_UNKNOWN;
	}
	if (resp.hash_type != EC_VBOOT_HASH_TYPE_SHA256) {
		printf("EC hash was the wrong type.\n");
		return VBERROR_UNKNOWN;
	}

	*hash = resp.hash_digest;
	*hash_size = resp.digest_size;

	return VBERROR_SUCCESS;
}

VbError_t VbExEcGetExpectedImage(int devidx, enum VbSelectFirmware_t select,
				 const uint8_t **image, int *image_size)
{
	const char *name;
	const char *main_name;

	switch (select) {
	case VB_SELECT_FIRMWARE_A:
		name = (devidx == 0 ? "EC_MAIN_A" : "PD_MAIN_A");
		main_name = "FW_MAIN_A";
		break;
	case VB_SELECT_FIRMWARE_B:
		name = (devidx == 0 ? "EC_MAIN_B" : "PD_MAIN_B");
		main_name = "FW_MAIN_B";
		break;
	case VB_SELECT_FIRMWARE_READONLY:
		name = "FW_MAIN_RO";
		main_name = "FW_MAIN_RO";
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
		const char *filename = get_fw_filename(devidx, select);
		struct cbfs_file *file =
			get_file_from_cbfs(main_name, filename);
		if (file == NULL)
			return VBERROR_UNKNOWN;
		printf("found '%s', sized 0x%x\n", filename, ntohl(file->len));
		*image = CBFS_SUBHEADER(file);
		*image_size = ntohl(file->len);
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

static VbError_t ec_protect_region(int devidx, enum VbSelectFirmware_t select,
				   int protect)
{
	struct ec_response_flash_protect resp;
	uint32_t protected_region = EC_FLASH_PROTECT_ALL_NOW;
	uint32_t mask = EC_FLASH_PROTECT_ALL_NOW | EC_FLASH_PROTECT_ALL_AT_BOOT;

	if (select == VB_SELECT_FIRMWARE_READONLY)
		protected_region = EC_FLASH_PROTECT_RO_NOW;

	/* Update protection */
	if (cros_ec_flash_protect(devidx, mask,
				  protect ? mask : 0, &resp) < 0) {
		printf("Failed to update EC flash protection.\n");
		return VBERROR_UNKNOWN;
	}

	if (!protect) {
		/* If protection is still enabled, need reboot */
		if (resp.flags & protected_region)
			return VBERROR_EC_REBOOT_TO_RO_REQUIRED;

		return VBERROR_SUCCESS;
	}

	/*
	 * If write protect and ro-at-boot aren't both asserted, don't expect
	 * protection enabled.
	 */
	if ((~resp.flags) & (EC_FLASH_PROTECT_GPIO_ASSERTED |
			     EC_FLASH_PROTECT_RO_AT_BOOT))
		return VBERROR_SUCCESS;

	/* If flash is protected now, success */
	if (resp.flags & EC_FLASH_PROTECT_ALL_NOW)
		return VBERROR_SUCCESS;

	/* If RW will be protected at boot but not now, need a reboot */
	if (resp.flags & EC_FLASH_PROTECT_ALL_AT_BOOT)
		return VBERROR_EC_REBOOT_TO_RO_REQUIRED;

	/* Otherwise, it's an error */
	return VBERROR_UNKNOWN;
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
	if (fmap_find_area(name, &area)) {
		printf("Didn't find section %s in the fmap.\n", name);
		return VBERROR_UNKNOWN;
	}

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
		*hash = get_file_hash_from_cbfs(name, filename, hash_size);
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
	int rv = ec_protect_region(devidx, select, 0);
	if (rv == VBERROR_EC_REBOOT_TO_RO_REQUIRED || rv != VBERROR_SUCCESS)
		return rv;

	if (cros_ec_flash_update_region(devidx, get_flash_region(select), image,
					image_size)) {
		printf("Failed to update EC-%s flash.\n", select ==
		       VB_SELECT_FIRMWARE_READONLY ? "RO" : "RW");
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

VbError_t VbExEcProtect(int devidx, enum VbSelectFirmware_t select)
{
	return ec_protect_region(devidx, select, 1);
}

VbError_t VbExEcEnteringMode(int devidx, enum VbEcBootMode_t mode)
{
	switch(mode) {
	case VB_EC_RECOVERY:
		return cros_ec_entering_mode(devidx, VBOOT_MODE_RECOVERY);
	case VB_EC_DEVELOPER:
		return cros_ec_entering_mode(devidx, VBOOT_MODE_DEVELOPER);
	case VB_EC_NORMAL:
	default :
		return cros_ec_entering_mode(devidx, VBOOT_MODE_NORMAL);
	}
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

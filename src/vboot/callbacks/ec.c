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
#include <vboot_api.h>

#include "drivers/ec/cros/ec.h"
#include "drivers/flash/flash.h"
#include "image/fmap.h"
#include "image/index.h"
#include "vboot/util/flag.h"

int VbExTrustEC(void)
{
	int val = flag_fetch(FLAG_ECINRW);
	if (val < 0) {
		printf("Couldn't tell if the EC is running RW firmware.\n");
		return 0;
	}
	// Trust the EC if it's NOT in its RW firmware.
	return !val;
}

VbError_t VbExEcRunningRW(int *in_rw)
{
	enum ec_current_image image;
	if (cros_ec_read_current_image(&image) < 0) {
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

VbError_t VbExEcJumpToRW(void)
{
	if (cros_ec_reboot(EC_REBOOT_JUMP_RW, 0) < 0) {
		printf("Failed to make the EC jump to RW.\n");
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

VbError_t VbExEcDisableJump(void)
{
	if (cros_ec_reboot(EC_REBOOT_DISABLE_JUMP, 0) < 0) {
		printf("Failed to make the EC disable jumping.\n");
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

VbError_t VbExEcHashRW(const uint8_t **hash, int *hash_size)
{
	static struct ec_response_vboot_hash resp;
	if (cros_ec_read_hash(&resp) < 0) {
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

VbError_t VbExEcGetExpectedRW(enum VbSelectFirmware_t select,
			      const uint8_t **image, int *image_size)
{
	const char *name;

	switch (select) {
	case VB_SELECT_FIRMWARE_A:
		name = "EC_MAIN_A";
		break;
	case VB_SELECT_FIRMWARE_B:
		name = "EC_MAIN_B";
		break;
	default:
		printf("Unrecognized EC firmware requested.\n");
		return VBERROR_UNKNOWN;
	}

	FmapArea area;
	if (fmap_find_area(name, &area)) {
		printf("Didn't find section %s in the fmap.\n", name);
		return VBERROR_UNKNOWN;
	}

	uint32_t size;
	*image = index_subsection(&area, 0, &size);
	*image_size = size;
	if (!*image)
		return VBERROR_UNKNOWN;

	printf("EC-RW firmware address, size are %p, %d.\n",
		*image, *image_size);

	return VBERROR_SUCCESS;
}

static VbError_t ec_protect_rw(int protect)
{
	struct ec_response_flash_protect resp;
	uint32_t mask = EC_FLASH_PROTECT_ALL_NOW | EC_FLASH_PROTECT_ALL_AT_BOOT;

	/* Update protection */
	if (cros_ec_flash_protect(mask, protect ? mask : 0, &resp) < 0) {
		printf("Failed to update EC flash protection.\n");
		return VBERROR_UNKNOWN;
	}

	if (!protect) {
		/* If protection is still enabled, need reboot */
		if (resp.flags & EC_FLASH_PROTECT_ALL_NOW)
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

VbError_t VbExEcGetExpectedRWHash(enum VbSelectFirmware_t select,
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
	*hash = index_subsection(&area, 1, &size);
	*hash_size = size;
	if (!*hash)
		return VBERROR_UNKNOWN;

	printf("EC-RW hash address, size are %p, %d.\n",
		*hash, *hash_size);

	printf("Hash = ");
	for (int i = 0; i < *hash_size; i++)
		printf("%02x", (*hash)[i]);
	printf("\n");

	return VBERROR_SUCCESS;
}

VbError_t VbExEcUpdateRW(const uint8_t *image, int image_size)
{
	int rv = ec_protect_rw(0);
	if (rv == VBERROR_EC_REBOOT_TO_RO_REQUIRED || rv != VBERROR_SUCCESS)
		return rv;

	if (cros_ec_flash_update_rw(image, image_size)) {
		printf("Failed to update EC RW flash.\n");
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

VbError_t VbExEcProtectRW(void)
{
	return ec_protect_rw(1);
}

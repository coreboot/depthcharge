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
#include <cbfs.h>
#include <libpayload.h>
#include <stddef.h>
#include <vboot_api.h>

#include "base/timestamp.h"
#include "drivers/ec/vboot_aux_fw.h"
#include "drivers/ec/vboot_ec.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/cbfs.h"
#include "image/fmap.h"
#include "vboot/util/flag.h"

#define _EC_FILENAME(devidx, select, suffix) \
	(select == VB_SELECT_FIRMWARE_READONLY ? "ecro" suffix : \
	 (devidx == 0 ? "ecrw" suffix : "pdrw" suffix))
#define EC_IMAGE_FILENAME(devidx, select) _EC_FILENAME(devidx, select, "")
#define EC_HASH_FILENAME(devidx, select) _EC_FILENAME(devidx, select, ".hash")

static struct cbfs_media *ro_cbfs;

static void *get_file_from_cbfs(
	const char *filename, enum VbSelectFirmware_t select, size_t *size)
{
	if (!CONFIG(DRIVER_CBFS_FLASH))
		return NULL;

	if (select == VB_SELECT_FIRMWARE_READONLY) {
		printf("Trying to locate '%s' in RO CBFS\n", filename);
		if (ro_cbfs == NULL)
			ro_cbfs = cbfs_ro_media();
		return cbfs_get_file_content(ro_cbfs, filename,
					     CBFS_TYPE_RAW, size);
	}

	printf("Trying to locate '%s' in CBFS\n", filename);
	return cbfs_get_file_content(CBFS_DEFAULT_MEDIA, filename,
				     CBFS_TYPE_RAW, size);
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
	VbootEcOps *ec = vboot_get_ec(devidx);
	assert(ec && ec->running_rw);
	return ec->running_rw(ec, in_rw);
}

VbError_t VbExEcJumpToRW(int devidx)
{
	VbootEcOps *ec = vboot_get_ec(devidx);
	assert(ec && ec->jump_to_rw);
	return ec->jump_to_rw(ec);
}

VbError_t VbExEcDisableJump(int devidx)
{
	VbootEcOps *ec = vboot_get_ec(devidx);
	assert(ec && ec->disable_jump);
	return ec->disable_jump(ec);
}

VbError_t VbExEcHashImage(int devidx, enum VbSelectFirmware_t select,
			  const uint8_t **hash, int *hash_size)
{
	VbootEcOps *ec = vboot_get_ec(devidx);
	assert(ec && ec->hash_image);
	return ec->hash_image(ec, select, hash, hash_size);
}

VbError_t VbExEcGetExpectedImage(int devidx, enum VbSelectFirmware_t select,
				 const uint8_t **image, int *image_size)
{
	size_t size;
	const char *filename = EC_IMAGE_FILENAME(devidx, select);
	*image = get_file_from_cbfs(filename, select, &size);
	if (*image == NULL)
		return VBERROR_UNKNOWN;
	*image_size = size;
	return VBERROR_SUCCESS;
}

VbError_t VbExEcGetExpectedImageHash(int devidx, enum VbSelectFirmware_t select,
				     const uint8_t **hash, int *hash_size)
{
	size_t size;
	const char *filename = EC_HASH_FILENAME(devidx, select);
	*hash = get_file_from_cbfs(filename, select, &size);
	if (!*hash)
		return VBERROR_UNKNOWN;
	*hash_size = size;

	return VBERROR_SUCCESS;
}

VbError_t VbExEcUpdateImage(int devidx, enum VbSelectFirmware_t select,
			    const uint8_t *image, int image_size)
{
	VbootEcOps *ec = vboot_get_ec(devidx);
	assert(ec && ec->update_image);
	return ec->update_image(ec, select, image, image_size);
}

VbError_t VbExEcProtect(int devidx, enum VbSelectFirmware_t select)
{
	VbootEcOps *ec = vboot_get_ec(devidx);
	assert(ec && ec->protect);
	return ec->protect(ec, select);
}

VbError_t VbExEcEnteringMode(int devidx, enum VbEcBootMode_t mode)
{
	VbootEcOps *ec = vboot_get_ec(devidx);
	assert(ec && ec->entering_mode);
	return ec->entering_mode(ec, mode);
}

/* Wait 3 seconds after software sync for EC to clear the limit power flag. */
#define LIMIT_POWER_WAIT_TIMEOUT 3000
/* Check the limit power flag every 50 ms while waiting. */
#define LIMIT_POWER_POLL_SLEEP 50

VbError_t VbExEcVbootDone(int in_recovery)
{
	VbootEcOps *ec = vboot_get_ec(PRIMARY_VBOOT_EC);
	int limit_power;
	int limit_power_wait_time = 0;
	int message_printed = 0;

	/*
	 * The entire EC SW Sync including the AUX FW update is complete at
	 * this point. AP firmware won't need to communicate to peripherals
	 * past this point, so protect the remote bus/tunnel to prevent OS from
	 * accessing it later.
	 *
	 * Also doing this here instead of the TCPC(AUX FW) sync code because
	 * some chips that do not require FW update do not register with AUX FW
	 * driver. Doing this here will help protect those tunnels as well.
	 */
	if (ec->protect_tcpc_ports && ec->protect_tcpc_ports(ec)) {
		printf("Some remote tunnels in EC may be unprotected\n");
		return VBERROR_UNKNOWN;
	}

	/* Ensure we have enough power to continue booting */
	while(1) {
		if (ec->check_limit_power(ec, &limit_power)) {
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
	VbootEcOps *ec = vboot_get_ec(PRIMARY_VBOOT_EC);
	return (ec->battery_cutoff(ec) == 0
		 ? VBERROR_SUCCESS : VBERROR_UNKNOWN);
}

VbError_t VbExCheckAuxFw(VbAuxFwUpdateSeverity_t *severity)
{
	return check_vboot_aux_fw(severity);
}

VbError_t VbExUpdateAuxFw(void)
{
	return update_vboot_aux_fw();
}

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
#include <vb2_api.h>
#include <vboot_api.h>
#include <2misc.h>

#include "base/timestamp.h"
#include "drivers/ec/vboot_ec.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/cbfs.h"
#include "image/fmap.h"
#include "vboot/util/flag.h"

#define _EC_FILENAME(select, suffix) \
	(select == VB_SELECT_FIRMWARE_READONLY ? "ecro" suffix : \
	 "ecrw" suffix)
#define EC_IMAGE_FILENAME(select) _EC_FILENAME(select, "")
#define EC_HASH_FILENAME(select) _EC_FILENAME(select, ".hash")

static struct cbfs_media *ro_cbfs;

static void *get_file_from_cbfs(
	const char *filename, enum vb2_firmware_selection select, size_t *size)
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

int vb2ex_ec_trusted(void)
{
	int val;

	val = flag_fetch(FLAG_ECINRW);
	if (val < 0) {
		printf("Couldn't tell if the EC is running RW firmware.\n");
		return 0;
	}
	// Trust the EC if it's NOT in its RW firmware.
	return !val;
}

vb2_error_t vb2ex_ec_running_rw(int *in_rw)
{
	VbootEcOps *ec = vboot_get_ec(PRIMARY_VBOOT_EC);
	assert(ec && ec->running_rw);
	return ec->running_rw(ec, in_rw);
}

vb2_error_t vb2ex_ec_jump_to_rw(void)
{
	VbootEcOps *ec = vboot_get_ec(PRIMARY_VBOOT_EC);
	assert(ec && ec->jump_to_rw);
	return ec->jump_to_rw(ec);
}

vb2_error_t vb2ex_ec_disable_jump(void)
{
	VbootEcOps *ec = vboot_get_ec(PRIMARY_VBOOT_EC);
	assert(ec && ec->disable_jump);
	return ec->disable_jump(ec);
}

vb2_error_t vb2ex_ec_hash_image(enum vb2_firmware_selection select,
				const uint8_t **hash, int *hash_size)
{
	VbootEcOps *ec = vboot_get_ec(PRIMARY_VBOOT_EC);
	assert(ec && ec->hash_image);
	return ec->hash_image(ec, select, hash, hash_size);
}

vb2_error_t vb2ex_ec_get_expected_image_hash(enum vb2_firmware_selection select,
					     const uint8_t **hash,
					     int *hash_size)
{
	size_t size;
	const char *filename = EC_HASH_FILENAME(select);
	*hash = get_file_from_cbfs(filename, select, &size);
	if (!*hash)
		return VB2_ERROR_UNKNOWN;
	*hash_size = size;

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_ec_update_image(enum vb2_firmware_selection select)
{
	VbootEcOps *ec = vboot_get_ec(PRIMARY_VBOOT_EC);
	const char *filename = EC_IMAGE_FILENAME(select);
	size_t size;
	uint8_t *image = get_file_from_cbfs(filename, select, &size);
	if (image == NULL)
		return VB2_ERROR_UNKNOWN;

	assert(ec && ec->update_image);
	return ec->update_image(ec, select, image, size);
}

vb2_error_t vb2ex_ec_protect(enum vb2_firmware_selection select)
{
	VbootEcOps *ec = vboot_get_ec(PRIMARY_VBOOT_EC);
	assert(ec && ec->protect);
	return ec->protect(ec, select);
}

/* Wait 3 seconds after software sync for EC to clear the limit power flag. */
#define LIMIT_POWER_WAIT_TIMEOUT 3000
/* Check the limit power flag every 50 ms while waiting. */
#define LIMIT_POWER_POLL_SLEEP 50

vb2_error_t vb2ex_ec_vboot_done(struct vb2_context *ctx)
{
	VbootEcOps *ec = vboot_get_ec(PRIMARY_VBOOT_EC);
	int limit_power;
	int limit_power_wait_time = 0;
	int message_printed = 0;
	int in_recovery = !!(ctx->flags & VB2_CONTEXT_RECOVERY_MODE);

	/* Ensure we have enough power to continue booting */
	while(1) {
		if (ec->check_limit_power(ec, &limit_power)) {
			printf("Failed to check EC limit power flag.\n");
			return VB2_ERROR_UNKNOWN;
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
	return VB2_SUCCESS;
}

vb2_error_t vb2ex_ec_battery_cutoff(void)
{
	VbootEcOps *ec = vboot_get_ec(PRIMARY_VBOOT_EC);
	return (ec->battery_cutoff(ec) == 0
		 ? VB2_SUCCESS : VB2_ERROR_UNKNOWN);
}

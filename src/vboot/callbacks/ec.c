/*
 * Copyright 2012 Google LLC
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

#include "base/timestamp.h"
#include "drivers/ec/vboot_ec.h"
#include "drivers/flash/flash.h"
#include "image/fmap.h"
#include "vboot/context.h"
#include "vboot/ui.h"
#include "vboot/util/flag.h"

#define _EC_FILENAME(select, suffix) \
	(select == VB_SELECT_FIRMWARE_READONLY ? "ecro" suffix : \
	 "ecrw" suffix)
#define EC_IMAGE_FILENAME(select) _EC_FILENAME(select, "")
#define EC_HASH_FILENAME(select) _EC_FILENAME(select, ".hash")

static void *get_file_from_cbfs(
	const char *filename, enum vb2_firmware_selection select, size_t *size)
{
	if (!CONFIG(DRIVER_CBFS_FLASH))
		return NULL;

	if (select == VB_SELECT_FIRMWARE_READONLY) {
		printf("Trying to locate '%s' in RO CBFS\n", filename);
		return cbfs_ro_map(filename, size);
	}

	printf("Trying to locate '%s' in CBFS\n", filename);
	return cbfs_map(filename, size);
}

vb2_error_t vb2ex_ec_running_rw(int *in_rw)
{
	VbootEcOps *ec = vboot_get_ec();
	assert(ec && ec->running_rw);
	return ec->running_rw(ec, in_rw);
}

vb2_error_t vb2ex_ec_jump_to_rw(void)
{
	VbootEcOps *ec = vboot_get_ec();
	assert(ec && ec->jump_to_rw);
	return ec->jump_to_rw(ec);
}

vb2_error_t vb2ex_ec_disable_jump(void)
{
	VbootEcOps *ec = vboot_get_ec();
	assert(ec && ec->disable_jump);
	return ec->disable_jump(ec);
}

vb2_error_t vb2ex_ec_hash_image(enum vb2_firmware_selection select,
				const uint8_t **hash, int *hash_size)
{
	VbootEcOps *ec = vboot_get_ec();
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
	/* Display firmware sync screen if update is slow */
	if (CONFIG(EC_SLOW_UPDATE)) {
		struct ui_context ui;
		struct vb2_context *ctx = vboot_get_context();
		if (vb2api_need_reboot_for_display(ctx))
			return VB2_REQUEST_REBOOT;
		printf("EC is updating. Show firmware sync screen.\n");
		if (ui_init_context(&ui, ctx, UI_SCREEN_FIRMWARE_SYNC) ==
		    VB2_SUCCESS)
			ui_display(&ui, NULL);
		else
			printf("Failed to initialize UI context.\n");
	}

	int32_t start_ts = vb2ex_mtime();
	VbootEcOps *ec = vboot_get_ec();
	const char *filename = EC_IMAGE_FILENAME(select);
	size_t size;
	uint8_t *image = get_file_from_cbfs(filename, select, &size);
	if (image == NULL)
		return VB2_ERROR_UNKNOWN;

	assert(ec && ec->update_image);
	vb2_error_t rv = ec->update_image(ec, select, image, size);
	printf("%s: Finished in %u ms\n", __func__, vb2ex_mtime() - start_ts);
	free(image);

	return rv;
}

vb2_error_t vb2ex_ec_protect(void)
{
	VbootEcOps *ec = vboot_get_ec();
	assert(ec && ec->protect);
	return ec->protect(ec);
}

/* Wait 3 seconds after software sync for EC to clear the limit power flag. */
#define LIMIT_POWER_WAIT_TIMEOUT 3000
/* Check the limit power flag every 50 ms while waiting. */
#define LIMIT_POWER_POLL_SLEEP 50

vb2_error_t vb2ex_ec_vboot_done(struct vb2_context *ctx)
{
	VbootEcOps *ec = vboot_get_ec();
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
		return VB2_REQUEST_SHUTDOWN;
	}

	timestamp_add_now(TS_VB_EC_VBOOT_DONE);
	return VB2_SUCCESS;
}

vb2_error_t vb2ex_ec_battery_cutoff(void)
{
	VbootEcOps *ec = vboot_get_ec();

	assert(ec && ec->battery_cutoff);
	return (ec->battery_cutoff(ec) == 0
		? VB2_SUCCESS : VB2_ERROR_UNKNOWN);
}

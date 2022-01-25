/*
 * Copyright 2017 Google Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <cbfs.h>
#include <libpayload.h>
#include <vb2_api.h>
#include <vboot_api.h>

#include "drivers/ec/cros/ec.h"
#include "drivers/ec/vboot_auxfw.h"
#include "vboot/ui.h"
#include "vboot/util/commonparams.h"

static struct {
	const VbootAuxfwOps *fw_ops;
	enum vb2_auxfw_update_severity severity;
} vboot_auxfw[NUM_MAX_VBOOT_AUXFW];

static int vboot_auxfw_count = 0;

void register_vboot_auxfw(const VbootAuxfwOps *auxfw)
{
	if (vboot_auxfw_count >= NUM_MAX_VBOOT_AUXFW)
		die("NUM_MAX_VBOOT_AUXFW (%d) too small\n",
		    NUM_MAX_VBOOT_AUXFW);

	vboot_auxfw[vboot_auxfw_count++].fw_ops = auxfw;
}

/**
 * Check device firmware version.
 *
 * Figure out if we have to update a given device's firmware.  If we don't
 * have an update blob in cbfs for the device, refuse to continue.  Otherwise,
 * report back if it's going to be a "fast" or "slow" update.
 *
 * @param auxfw		FW device ops
 * @param severity	return parameter for health of auxfw
 * @return VB2_SUCCESS, or non-zero if error.
 */

static vb2_error_t check_dev_fw_hash(const VbootAuxfwOps *auxfw,
				     enum vb2_auxfw_update_severity *severity)
{
	const void *want_hash;
	size_t want_size;

	/* find bundled fw hash */
	want_hash = cbfs_get_file_content(
		CBFS_DEFAULT_MEDIA,
		auxfw->fw_hash_name, CBFS_TYPE_RAW, &want_size);
	if (want_hash == NULL) {
		printf("%s missing from CBFS\n", auxfw->fw_hash_name);
		return VB2_ERROR_UNKNOWN;
	}

	return auxfw->check_hash(auxfw, want_hash, want_size, severity);
}

vb2_error_t check_vboot_auxfw(enum vb2_auxfw_update_severity *severity)
{
	enum vb2_auxfw_update_severity max;
	enum vb2_auxfw_update_severity current;
	vb2_error_t status;

	if (CONFIG(CROS_EC_PROBE_AUX_FW_INFO))
		cros_ec_probe_aux_fw_chips();
	max = VB2_AUXFW_NO_DEVICE;
	for (int i = 0; i < vboot_auxfw_count; ++i) {
		const VbootAuxfwOps *const auxfw = vboot_auxfw[i].fw_ops;

		status = check_dev_fw_hash(auxfw, &current);
		if (status != VB2_SUCCESS)
			return status;

		vboot_auxfw[i].severity = current;
		max = MAX(max, current);
	}

	*severity = max;
	return VB2_SUCCESS;
}

/**
 * Display firmware sync screen if needed.
 *
 * When there'll be a slow update, try to display firmware sync screen. If
 * the display hasn't been initialized, request a reboot.
 *
 * @return VB2_SUCCESS, or non-zero if error.
 */
static vb2_error_t display_firmware_sync_screen(void)
{
	struct vb2_context *ctx = vboot_get_context();
	uint32_t locale;

	for (int i = 0; i < vboot_auxfw_count; ++i) {
		/* Display firmware sync screen if update is slow */
		if (vboot_auxfw[i].severity == VB2_AUXFW_SLOW_UPDATE) {
			if (vb2api_need_reboot_for_display(ctx))
				return VB2_REQUEST_REBOOT;

			locale = vb2api_get_locale_id(ctx);
			printf("AUXFW is updating. "
			       "Show firmware sync screen.\n");
			ui_display(UI_SCREEN_FIRMWARE_SYNC, locale,
				   0, 0, 0, 0, 0, UI_ERROR_NONE);

			break;
		}
	}

	return VB2_SUCCESS;
}

/**
 * Apply the device firmware update.
 *
 * @param auxfw		FW device ops
 * @return VB2_SUCCESS, or non-zero if error.
 */
static vb2_error_t apply_dev_fw(const VbootAuxfwOps *auxfw)
{
	const uint8_t *want_data;
	size_t want_size;

	/* find bundled fw */
	want_data = cbfs_get_file_content(
		CBFS_DEFAULT_MEDIA,
		auxfw->fw_image_name, CBFS_TYPE_RAW, &want_size);
	if (want_data == NULL) {
		printf("%s missing from CBFS\n", auxfw->fw_image_name);
		return VB2_ERROR_UNKNOWN;
	}
	return auxfw->update_image(auxfw, want_data, want_size);
}

vb2_error_t update_vboot_auxfw(void)
{
	enum vb2_auxfw_update_severity severity;
	vb2_error_t status = VB2_SUCCESS;
	int lid_shutdown_disabled = 0;

	VB2_TRY(display_firmware_sync_screen());

	for (int i = 0; i < vboot_auxfw_count; ++i) {
		const VbootAuxfwOps *auxfw;

		auxfw = vboot_auxfw[i].fw_ops;
		if (vboot_auxfw[i].severity == VB2_AUXFW_NO_DEVICE)
			continue;

		if (vboot_auxfw[i].severity != VB2_AUXFW_NO_UPDATE) {
			/* Disable lid shutdown on x86 if enabled */
			if (!lid_shutdown_disabled &&
			    CONFIG(ARCH_X86) &&
			    CONFIG(DRIVER_EC_CROS) &&
			    cros_ec_get_lid_shutdown_mask() > 0) {
				if (!cros_ec_set_lid_shutdown_mask(0))
					lid_shutdown_disabled = 1;
			}

			/* Apply update */
			printf("Update auxfw %d\n", i);
			status = apply_dev_fw(auxfw);
			if (status == VB2_ERROR_EX_AUXFW_PERIPHERAL_BUSY) {
				status = VB2_SUCCESS;
				continue;
			} else if (status != VB2_SUCCESS) {
				break;
			}

			/* Re-check hash after update */
			status = check_dev_fw_hash(auxfw, &severity);
			if (status != VB2_SUCCESS)
				break;
			if (severity != VB2_AUXFW_NO_UPDATE) {
				status = VB2_ERROR_UNKNOWN;
				break;
			}
		}
	}

	/* Re-enable lid shutdown event, if required */
	if (CONFIG(DRIVER_EC_CROS) && lid_shutdown_disabled)
		cros_ec_set_lid_shutdown_mask(1);

	return status;
}

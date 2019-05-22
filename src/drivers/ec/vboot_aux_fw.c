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

#include "drivers/ec/cros/ec.h"
#include "drivers/ec/vboot_aux_fw.h"

#include <libpayload.h>

#include <cbfs.h>

static struct {
	const VbootAuxFwOps *fw_ops;
	VbAuxFwUpdateSeverity_t severity;
} vboot_aux_fw[NUM_MAX_VBOOT_AUX_FW];

static int vboot_aux_fw_count = 0;

/**
 * register a firmware updater
 *
 * @param aux_fw	descriptor of updater ops
 * @return		die()s on failure
 */

void register_vboot_aux_fw(const VbootAuxFwOps *aux_fw)
{
	if (vboot_aux_fw_count >= NUM_MAX_VBOOT_AUX_FW)
		die("NUM_MAX_VBOOT_AUX_FW (%d) too small\n",
		    NUM_MAX_VBOOT_AUX_FW);

	vboot_aux_fw[vboot_aux_fw_count++].fw_ops = aux_fw;
}

/**
 * check device firmware version
 *
 * figure out if we have to update a given device's firmware.  if we
 * don't have an update blob in cbfs for the device, refuse to
 * continue.  else, report back if it's going to be a "fast" or "slow"
 * update.
 *
 * @param aux_fw	FW device ops
 * @param severity	return parameter for health of auxiliary firmware
 * @return VBERROR_... error, VBERROR_SUCCESS on success.
 */

static VbError_t check_dev_fw_hash(const VbootAuxFwOps *aux_fw,
				   VbAuxFwUpdateSeverity_t *severity)
{
	const void *want_hash;
	size_t want_size;

	/* find bundled fw hash */
	want_hash = cbfs_get_file_content(
		CBFS_DEFAULT_MEDIA,
		aux_fw->fw_hash_name, CBFS_TYPE_RAW, &want_size);
	if (want_hash == NULL) {
		printf("%s missing from CBFS\n", aux_fw->fw_hash_name);
		return VBERROR_UNKNOWN;
	}

	return aux_fw->check_hash(aux_fw, want_hash, want_size, severity);
}

/**
 * iterate over registered firmware updaters to determine what needs to
 * be updated.  returns how slow the worst case update will be.
 *
 * @param severity	returns VB_AUX_FW_{NO,FAST,SLOW}_UPDATE for worst case
 * @return VBERROR_... error, VBERROR_SUCCESS on success.
 */

VbError_t check_vboot_aux_fw(VbAuxFwUpdateSeverity_t *severity)
{
	VbAuxFwUpdateSeverity_t max;
	VbAuxFwUpdateSeverity_t current;
	VbError_t status;

	max = VB_AUX_FW_NO_DEVICE;
	for (int i = 0; i < vboot_aux_fw_count; ++i) {
		const VbootAuxFwOps *const aux_fw = vboot_aux_fw[i].fw_ops;

		status = check_dev_fw_hash(aux_fw, &current);
		if (status != VBERROR_SUCCESS)
			return status;

		vboot_aux_fw[i].severity = current;
		max = MAX(max, current);
	}

	*severity = max;
	return VBERROR_SUCCESS;
}

/**
 * apply the device firmware update
 *
 * @param aux_fw	FW device ops
 * @return VBERROR_... error, VBERROR_SUCCESS on success.
 */

static VbError_t apply_dev_fw(const VbootAuxFwOps *aux_fw)
{
	const uint8_t *want_data;
	size_t want_size;

	/* find bundled fw */
	want_data = cbfs_get_file_content(
		CBFS_DEFAULT_MEDIA,
		aux_fw->fw_image_name, CBFS_TYPE_RAW, &want_size);
	if (want_data == NULL) {
		printf("%s missing from CBFS\n", aux_fw->fw_image_name);
		return VBERROR_UNKNOWN;
	}
	return aux_fw->update_image(aux_fw, want_data, want_size);
}

/**
 * iterate over registered firmware updaters and apply updates where
 * needed.  check_vboot_aux_fw() must have been called before this to
 * determine what needs to be updated.
 *
 * @return VBERROR_... error, VBERROR_SUCCESS on success.
 */

VbError_t update_vboot_aux_fw(void)
{
	VbAuxFwUpdateSeverity_t severity;
	VbError_t status = VBERROR_SUCCESS;
	int lid_shutdown_disabled = 0;

	for (int i = 0; i < vboot_aux_fw_count; ++i) {
		const VbootAuxFwOps *aux_fw;

		aux_fw = vboot_aux_fw[i].fw_ops;
		if (vboot_aux_fw[i].severity == VB_AUX_FW_NO_DEVICE)
			continue;

		if (vboot_aux_fw[i].severity != VB_AUX_FW_NO_UPDATE) {
			/* Disable lid shutdown on x86 if enabled */
			if (!lid_shutdown_disabled &&
			    CONFIG(ARCH_X86) &&
			    CONFIG(DRIVER_EC_CROS) &&
			    cros_ec_get_lid_shutdown_mask() > 0) {
				if (!cros_ec_set_lid_shutdown_mask(0))
					lid_shutdown_disabled = 1;
			}

			/* Apply update */
			printf("Update aux fw %d\n", i);
			status = apply_dev_fw(aux_fw);
			if (status == VBERROR_PERIPHERAL_BUSY) {
				status = VBERROR_SUCCESS;
				continue;
			} else if (status != VBERROR_SUCCESS) {
				break;
			}

			/* Re-check hash after update */
			status = check_dev_fw_hash(aux_fw, &severity);
			if (status != VBERROR_SUCCESS)
				break;
			if (severity != VB_AUX_FW_NO_UPDATE) {
				status = VBERROR_UNKNOWN;
				break;
			}
		}
	}

	/* Re-enable lid shutdown event, if required */
	if (CONFIG(DRIVER_EC_CROS) && lid_shutdown_disabled)
		cros_ec_set_lid_shutdown_mask(1);

	return status;
}

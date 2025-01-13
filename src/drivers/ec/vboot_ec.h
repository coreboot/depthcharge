/*
 * Copyright 2016 Google LLC
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

#ifndef __DRIVERS_EC_VBOOT_EC_H
#define __DRIVERS_EC_VBOOT_EC_H

#include <vb2_api.h>
#include <vboot_api.h>

typedef struct VbootEcOps {
	/* Directly correspond to normal vboot vb2ex_ec... interfaces. */
	vb2_error_t (*running_rw)(struct VbootEcOps *me, int *in_rw);
	vb2_error_t (*jump_to_rw)(struct VbootEcOps *me);
	vb2_error_t (*disable_jump)(struct VbootEcOps *me);
	vb2_error_t (*hash_image)(struct VbootEcOps *me,
				enum vb2_firmware_selection select,
				const uint8_t **hash, int *hash_size);
	vb2_error_t (*update_image)(struct VbootEcOps *me,
				  enum vb2_firmware_selection select,
				  const uint8_t *image, int image_size);
	vb2_error_t (*protect)(struct VbootEcOps *me,
			     enum vb2_firmware_selection select);

	/* Tells the EC to reboot to RO on next AP shutdown. */
	vb2_error_t (*reboot_to_ro)(struct VbootEcOps *me);

	/* Tells the EC to reboot to switch RW slot. */
	vb2_error_t (*reboot_switch_rw)(struct VbootEcOps *me);

	/* Tells the EC to reboot and keep the AP off after the reboot. */
	vb2_error_t (*reboot_ap_off)(struct VbootEcOps *me);

	/*
	 * Tells the EC to cut off battery.  This is expected to take
	 * effect when the system shuts down, not immediately.
	 */
	vb2_error_t (*battery_cutoff)(struct VbootEcOps *me);

	/* Check for EC request to limit power */
	vb2_error_t (*check_limit_power)(struct VbootEcOps *me, int *limit_power);

	/* Enable or disable power button */
	vb2_error_t (*enable_power_button)(struct VbootEcOps *me, int enable);

	/*
	 * Protect bus/tunnels to TCPC ports. Optional operation, so check
	 * before invoking it.
	 */
	vb2_error_t (*protect_tcpc_ports)(struct VbootEcOps *me);
} VbootEcOps;

/*
 * vboot_get_ec returns the VbootEcOps that was registered with
 * register_vboot_ec.
 */
VbootEcOps *vboot_get_ec(void);

void register_vboot_ec(VbootEcOps *ec);

#endif	/* __DRIVERS_EC_VBOOT_EC_H */

/*
 * Copyright 2016 Google Inc.
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

#include <vboot_api.h>

typedef struct VbootEcOps {
	/* Directly correspond to normal vboot VbExEc... interfaces. */
	VbError_t (*running_rw)(struct VbootEcOps *me, int *in_rw);
	VbError_t (*jump_to_rw)(struct VbootEcOps *me);
	VbError_t (*disable_jump)(struct VbootEcOps *me);
	VbError_t (*hash_image)(struct VbootEcOps *me,
				enum VbSelectFirmware_t select,
				const uint8_t **hash, int *hash_size);
	VbError_t (*update_image)(struct VbootEcOps *me,
				  enum VbSelectFirmware_t select,
				  const uint8_t *image, int image_size);
	VbError_t (*protect)(struct VbootEcOps *me,
			     enum VbSelectFirmware_t select);
	VbError_t (*entering_mode)(struct VbootEcOps *me,
				   enum VbEcBootMode_t mode);

	/* Tells the EC to reboot to RO on next AP shutdown. */
	VbError_t (*reboot_to_ro)(struct VbootEcOps *me);

	/*
	 * Tells the EC to cut off battery.  This is expected to take
	 * effect when the system shuts down, not immediately.
	 */
	VbError_t (*battery_cutoff)(struct VbootEcOps *me);

	/* Check for EC request to limit power */
	VbError_t (*check_limit_power)(struct VbootEcOps *me, int *limit_power);

	/*
	 * Protect bus/tunnels to TCPC ports. Optional operation, so check
	 * before invoking it.
	 */
	VbError_t (*protect_tcpc_ports)(struct VbootEcOps *me);
} VbootEcOps;

#define NUM_MAX_VBOOT_ECS 2
#define PRIMARY_VBOOT_EC  0

VbootEcOps *vboot_get_ec(int devidx);
void register_vboot_ec(VbootEcOps *ec, int devidx);

#endif	/* __DRIVERS_EC_VBOOT_EC_H */

/*
 * Copyright 2017 Google Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_EC_VBOOT_AUXFW_H
#define __DRIVERS_EC_VBOOT_AUXFW_H

#include <stddef.h>
#include <stdint.h>

#include <vb2_api.h>
#include <vboot_api.h>

typedef struct VbootAuxfwOps VbootAuxfwOps;
struct VbootAuxfwOps {
	vb2_error_t (*check_hash)(const VbootAuxfwOps *me,
				const uint8_t *hash, size_t hash_size,
				enum vb2_auxfw_update_severity *severity);
	/*
	 * Return VB2_SUCCESS on successful update. Auxfw Sync in turn
	 * requests for an EC reboot to RO on successful update, so that any
	 * chip whose FW is updated gets reset to a clean state.
	 */
	vb2_error_t (*update_image)(const VbootAuxfwOps *me,
				  const uint8_t *image, size_t image_size);
	const char *fw_image_name;
	const char *fw_hash_name;
};

#define NUM_MAX_VBOOT_AUXFW 2

/**
 * Register a new firmware updater. The check_hash and update_image callbacks
 * need to be implemented for firmware update to work correctly.  If there
 * is no space for auxfw, it will call die().
 *
 * @param auxfw		Contains all the information and methods to update
 *			auxiliary firmware.
 */
void register_vboot_auxfw(const VbootAuxfwOps *auxfw);

/**
 * Iterate over registered firmware updaters to determine what needs to be
 * updated.  Returns how slow the worst case update will be.
 *
 * @param severity	returns VB2_AUXFW_{NO,FAST,SLOW}_UPDATE for worst case
 * @return VB2_SUCCESS, or non-zero if error.
 */
vb2_error_t check_vboot_auxfw(enum vb2_auxfw_update_severity *severity);

/**
 * Iterate over registered firmware updaters and apply updates where needed.
 * check_vboot_auxfw() must have been called before this to determine what
 * needs to be updated.
 *
 * @return VB2_SUCCESS, or non-zero if error.
 */
vb2_error_t update_vboot_auxfw(void);

#endif	/* __DRIVERS_EC_VBOOT_AUXFW_H */

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

#ifndef __DRIVERS_EC_VBOOT_AUX_FW_H
#define __DRIVERS_EC_VBOOT_AUX_FW_H

#include <stddef.h>
#include <stdint.h>

#include <vboot_api.h>

typedef struct VbootAuxFwOps VbootAuxFwOps;
struct VbootAuxFwOps {
	VbError_t (*check_hash)(const VbootAuxFwOps *me,
				const uint8_t *hash, size_t hash_size,
				VbAuxFwUpdateSeverity_t *severity);
	/*
	 * Return VBERROR_SUCCESS on successful update. AUX FW Sync in turn
	 * requests for an EC reboot to RO on successful update, so that any
	 * chip whose FW is updated gets reset to a clean state.
	 */
	VbError_t (*update_image)(const VbootAuxFwOps *me,
				  const uint8_t *image, size_t image_size);
	const char *fw_image_name;
	const char *fw_hash_name;
};

#define NUM_MAX_VBOOT_AUX_FW 2

extern void register_vboot_aux_fw(const VbootAuxFwOps *aux_fw);
extern VbError_t check_vboot_aux_fw(VbAuxFwUpdateSeverity_t *severity);
extern VbError_t update_vboot_aux_fw(void);

#endif	/* __DRIVERS_EC_VBOOT_AUX_FW_H */

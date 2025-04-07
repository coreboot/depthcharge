// SPDX-License-Identifier: GPL-2.0
/* Copyright 2024 Google LLC.  */

#ifndef __DRIVERS_EC_RTS5453_H__
#define __DRIVERS_EC_RTS5453_H__

#include "drivers/ec/cros/ec.h"

/**
 * @brief Board-provided function used by RTS5453 driver to obtain the FW
 *        binary and hash file to flash to the PDC. Board logic is used to
 *        determine the desired image to support multiple variations. A
 *        default implementation of this function returns NULL for both
 *        paths, which causes no update to occur.
 *
 * @param image_path Output pointer to a FW binary path string (or NULL)
 * @param hash_path Output pointer to a FW hash path string (or NULL)
 * @param ec_pd_id PD chip ID (from 0 to "#ports - 1")
 * @param r Pointer to the EC_CMD_PD_CHIP_INFO_V2 command response
 */
void board_rts5453_get_image_paths(const char **image_path, const char **hash_path,
				   int ec_pd_id, struct ec_response_pd_chip_info_v2 *r);

#endif /* __DRIVERS_EC_RTS5453_H__ */

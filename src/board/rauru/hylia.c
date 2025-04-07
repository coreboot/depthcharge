// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "drivers/ec/rts5453/rts5453.h"

/* Override of func in src/drivers/ec/rts5453/rts5453.c */
void board_rts5453_get_image_paths(const char **image_path, const char **hash_path,
				   int ec_pd_id, struct ec_response_pd_chip_info_v2 *r)
{
	*image_path = "rts5453_GOOG0600.bin";
	*hash_path = "rts5453_GOOG0600.hash";
}

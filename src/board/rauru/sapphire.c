// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "drivers/ec/rts5453/rts5453.h"

/* Override of func in src/drivers/ec/rts5453/rts5453.c */
void board_rts5453_get_image_paths(const char **image_path, const char **hash_path,
				   int ec_pd_id, struct ec_response_pd_chip_info_v2 *r)
{
	*image_path = "rts5453vb_GOOG0R00.bin";
	*hash_path = "rts5453vb_GOOG0R00.hash";
}

void board_rts545x_register(void)
{
	const uint32_t old_pid = 0x5065;
	static CrosEcAuxfwChipInfo rts5453_info = {
		.vid = CONFIG_DRIVER_EC_RTS545X_VID,
		.pid = old_pid,
		.new_chip_aux_fw_ops = new_rts545x_from_chip_info,
	};
	list_insert_after(&rts5453_info.list_node, &ec_aux_fw_chip_list);
}

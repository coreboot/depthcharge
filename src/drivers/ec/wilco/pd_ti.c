// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 *
 * Depthcharge driver and Verified Boot callbacks for the Wilco
 * PD Firmware update interface for TI TCPC.
 */

#include <libpayload.h>

#include "drivers/ec/wilco/pd.h"

static WilcoPdFlashInfo pd_flash_info_ti = {
	.flash_subtype = 0,
	.start_cmd = 0xe0,
	.erase_cmd = 0xe1,
	.write_cmd = 0xe2,
	.verify_cmd = 0xe3,
	.done_cmd = 0xe4,
};

WilcoPd *new_wilco_pd_ti(WilcoEc *ec, const char *fw_image_name,
			 const char *fw_hash_name)
{
	return new_wilco_pd(ec, &pd_flash_info_ti, fw_image_name, fw_hash_name);
}

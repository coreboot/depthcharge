/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2019 Google LLC
 *
 * Depthcharge driver and Verified Boot callbacks for the Wilco
 * PD Firmware update interface.
 */

#ifndef __DRIVERS_EC_WILCO_PD_TI_H__
#define __DRIVERS_EC_WILCO_PD_TI_H__

#include "drivers/ec/wilco/ec.h"
#include "drivers/ec/wilco/pd.h"

/**
 * new_wilco_pd_ti() - Prepare a Wilco PD TI firmware update object.
 * @ec: Wilco EC device.
 * @fw_image_name: Firmware image file name in CBFS.
 * @fw_hash_name: Firmware hash file name in CBFS.
 *
 * Return: Newly allocated Wilco PD object.
 */
WilcoPd *new_wilco_pd_ti(WilcoEc *ec, const char *fw_image_name,
			 const char *fw_hash_name);

#endif /* __DRIVERS_EC_WILCO_PD_TI_H__ */

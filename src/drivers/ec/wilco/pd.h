/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2019 Google LLC
 *
 * Depthcharge driver and Verified Boot callbacks for the Wilco
 * PD Firmware update interface.
 */

#ifndef __DRIVERS_EC_WILCO_PD_H__
#define __DRIVERS_EC_WILCO_PD_H__

#include "drivers/ec/wilco/ec.h"
#include "drivers/ec/wilco/flash.h"
#include "drivers/ec/vboot_aux_fw.h"

/**
 * WilcoPdFlashInfo - PD flash controller information
 * @flash_subtype: Unique subtype for this PD type/vendor.
 * @start_cmd: Mailbox command for flash start.
 * @erase_cmd: Mailbox command for flash erase.
 * @write_cmd: Mailbox command for flash write.
 * @verify_cmd: Mailbox command for flash verify.
 * @done_cmd: Mailbox command for flash done.
 */
typedef struct WilcoPdFlashInfo {
	uint8_t flash_subtype;
	uint8_t start_cmd;
	uint8_t erase_cmd;
	uint8_t write_cmd;
	uint8_t verify_cmd;
	uint8_t done_cmd;
} WilcoPdFlashInfo;

/**
 * WilcoPd - Wilco Embedded Controller PD firmware update
 * @ec: Handle for Wilco EC.
 * @ops: Handle for Verified Boot AUX firmware update callbacks.
 * @info: PD controller information &WilcoPdFlashInfo
 * @protected: Flag to indicate PD protect function has been called.
 */
typedef struct WilcoPd
{
	WilcoEc *ec;
	VbootAuxFwOps ops;
	WilcoPdFlashInfo *info;
	int protected;
} WilcoPd;

/**
 * new_wilco_pd() - Prepare a Wilco PD firmware update object.
 * @ec: Wilco EC device.
 * @info: PD controller info &WilcoPdFlashInfo.
 * @fw_image_name: Firmware image file name in CBFS.
 * @fw_hash_name: Firmware hash file name in CBFS.
 *
 * Return: Newly allocated Wilco PD object.
 */
WilcoPd *new_wilco_pd(WilcoEc *ec, WilcoPdFlashInfo *info,
		      const char *fw_image_name, const char *fw_hash_name);

#endif /* __DRIVERS_EC_WILCO_PD_H__ */

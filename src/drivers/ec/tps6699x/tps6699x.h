/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2024 Google LLC.  */

#ifndef __DRIVERS_EC_TPS6699X_H_
#define __DRIVERS_EC_TPS6699X_H_

#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/ec/common/pdc_utils.h"
#include "drivers/ec/vboot_auxfw.h"
#include "vb2_api.h"
#include "drivers/ec/cros/commands.h"

#if (TPS6699X_DEBUG > 0)
#define debug(msg, args...) printf("%s: " msg, __func__, ##args)
#else
#define debug(msg, args...)
#endif

typedef struct Tps6699x {
	/** This driver's API implementation, passed to Vboot */
	VbootAuxfwOps fw_ops;
	/** EC I2C tunnel used to access this chip */
	CrosECTunnelI2c *bus;
	/** these are cached from chip regs */
	struct {
		/** PDC's USB Vendor ID */
		uint16_t vid;
		/** PDC's USB Product ID */
		uint16_t pid;
		/** Reported FW version */
		pdc_fw_ver_t fw_version;
		/** I2C target address of this chip, use with Tps6699x::bus */
		uint8_t i2c_addr;
		/** Project Name */
		char fw_name_str[USB_PD_CHIP_INFO_PROJECT_NAME_LEN + 1];
	} chip_info;
	/** PD chip index as used by the EC */
	int ec_pd_id;
	/** Internally-used chip name for logging */
	char chip_name[16];
	/** I2C bus speed before FWUP to restore to original settings */
	uint16_t saved_i2c_speed_khz;
	/** Pointer to memory-mapped firmware binary file. The file is opened
	 *  handled by vboot and the pointer and size are provided through
	 *  VbootAuxfwOps::update_image().
	 */
	/** Pointer to memory-mapped firmware binary and its length */
	struct {
		const uint8_t *image;
		size_t size;
	} fw_image;
	/** Set true once the update completed */
	bool has_updated;
} Tps6699x;

const VbootAuxfwOps *new_tps6699x_from_chip_info(struct ec_response_pd_chip_info_v2 *r,
						 uint8_t ec_pd_id);

/**
 * @brief Board-provided function used by TPS6699x driver to obtain the FW
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
void board_tps6699x_get_image_paths(const char **image_path, const char **hash_path,
				   int ec_pd_id, struct ec_response_pd_chip_info_v2 *r);

#endif /* __DRIVERS_EC_TPS6699X_H_ */

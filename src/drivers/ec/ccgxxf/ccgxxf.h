/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2021 The Chromium OS Authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __DRIVERS_MISC_CCGXXF_H__
#define __DRIVERS_MISC_CCGXXF_H__

#include <stdint.h>

#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/vboot_auxfw.h"

#define CCGXXF_I2C_ADDR1_FLAGS	0x0B
#define CCGXXF_I2C_ADDR2_FLAGS	0x1B

#define CCGXXF_REG_VENDOR_ID	0x0
#define CCGXXF_REG_PRODUCT_ID	0x2

/* CCGXXF Firmware version location in binary image */
#define CCGXXF_BUILD_NUM_OFFSET		0xE0

#define CCGXXF_BUILD_VER_OFFSET		0xE2
#define CCGXXF_BUILD_VER_MINOR_OFFSET	0xE2
#define CCGXXF_BUILD_VER_MAJOR_OFFSET	0xE3

/* CCGXXF vendor info */
#define CCGXXF_VENDOR_ID		0x04B4
#define CCGXXF_PRODUCT_ID_CCG6DF	0xF6EE
#define CCGXXF_PRODUCT_ID_CCG6SF	0xF6EF

/* CCGXXF register definitions */
#define CCGXXF_REG_FWU_RESPONSE		0x90
#define CCGXXF_FWU_RES_RETRY		5
#define CCGXXF_FWU_STATUS_DELAY_MS	5
#define CCGXXF_FWU_VALIDATE_DELAY_MS	50
#define CCGXXF_FWU_RES_SUCCESS		0x0000
#define CCGXXF_FWU_RES_CMD_FAILED	0x0003
#define CCGXXF_FWU_RES_CMD_IN_PROGRESS	0x00FF
#define CCGXXF_FWU_RES_FW_REGION_1	0x0100
#define CCGXXF_FWU_RES_FW_REGION_2	0x0200

#define CCGXXF_REG_FWU_COMMAND		0x92
#define CCGXXF_FWU_PROG_CYCLE_DELAY_MS	20
#define CCGXXF_FWU_CMD_ENABLE		0x0011
#define CCGXXF_FWU_CMD_DISABLE		0x0022
#define CCGXXF_FWU_CMD_GET_FW_MODE	0x0033
#define CCGXXF_FWU_CMD_WRITE_FLASH_ROW	0x0044
#define CCGXXF_FWU_CMD_VALIDATE_FW_IMG	0x0066
#define CCGXXF_FWU_CMD_RESET		0x0077

#define CCGXXF_REG_FW_VERSION		0x94
#define CCGXXF_REG_FW_VERSION_BUILD	0x96

#define CCGXXF_REG_FWU_BUFFER		0x98
#define CCGXXF_FWU_BUFFER_ROW_SIZE	128

typedef struct Ccgxxf
{
	VbootAuxfwOps fw_ops;
	CrosECTunnelI2c *bus;
	int ec_pd_id;

	/* cached from chip regs */
	struct {
		uint16_t vendor;
		uint16_t product;
		uint16_t device;
		uint8_t fw_rev;
	} chip;
	char chip_name[16];
} Ccgxxf;

Ccgxxf *new_ccg6sf(CrosECTunnelI2c *bus, int ec_pd_id);
Ccgxxf *new_ccg6df(CrosECTunnelI2c *bus, int ec_pd_id);

#endif	/* __DRIVERS_MISC_CCGXXF_H__ */

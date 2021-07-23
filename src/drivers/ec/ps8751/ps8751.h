/*
 * Copyright 2017 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_EC_PS8751_H__
#define __DRIVERS_EC_PS8751_H__

#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/vboot_auxfw.h"

typedef enum  ParadeChipType {
	CHIP_PS8751,
	CHIP_PS8755,
	CHIP_PS8805,
	CHIP_PS8815,
	CHIP_PS8705,
} ParadeChipType;

typedef struct Ps8751
{
	VbootAuxfwOps fw_ops;
	CrosECTunnelI2c *bus;
	int ec_pd_id;

	/* these are cached from chip regs */
	struct {
		uint16_t vendor;
		uint16_t product;
		uint16_t device;
		uint8_t fw_rev;
	} chip;
	uint8_t blob_hw_version;
	ParadeChipType chip_type;
	char chip_name[16];
} Ps8751;

Ps8751 *new_ps8751(CrosECTunnelI2c *bus, int ec_pd_id);
Ps8751 *new_ps8751_canary(CrosECTunnelI2c *bus, int ec_pd_id);
Ps8751 *new_ps8755(CrosECTunnelI2c *bus, int ec_pd_id);
Ps8751 *new_ps8805_a2(CrosECTunnelI2c *bus, int ec_pd_id);
Ps8751 *new_ps8805_a3(CrosECTunnelI2c *bus, int ec_pd_id);
Ps8751 *new_ps8815_a0(CrosECTunnelI2c *bus, int ec_pd_id);
Ps8751 *new_ps8815_a1(CrosECTunnelI2c *bus, int ec_pd_id);

#endif /* __DRIVERS_EC_PS8751_H__ */

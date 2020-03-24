/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __DRIVERS_EC_ANX3429_H__
#define __DRIVERS_EC_ANX3429_H__

#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/vboot_auxfw.h"

typedef struct Anx3429
{
	VbootAuxfwOps fw_ops;
	CrosECTunnelI2c *bus;
	int ec_pd_id;
	int debug_updated;

	/* these are cached from chip regs */
	struct {
		uint16_t vendor;
		uint16_t product;
		uint16_t device;
		uint8_t fw_rev;
	} chip;
} Anx3429;

Anx3429 *new_anx3429(CrosECTunnelI2c *bus, int ec_pd_id);

#endif /* __DRIVERS_EC_ANX3429_H__ */

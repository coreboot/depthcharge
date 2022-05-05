/*
 * Copyright 2022 The Chromium OS Authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __DRIVERS_EC_ANX3447_H__
#define __DRIVERS_EC_ANX3447_H__

#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/vboot_aux_fw.h"

typedef struct Anx3447
{
	VbootAuxFwOps fw_ops;
	CrosECTunnelI2c *bus;
	int ec_pd_id;

	struct {
		uint16_t vendor;
		uint16_t product;
		uint16_t device;
		uint8_t fw_rev;
	} chip;

} Anx3447;

Anx3447 *new_anx3447(CrosECTunnelI2c *bus, int ec_pd_id);

#endif /* __DRIVERS_EC_ANX3447_H__ */

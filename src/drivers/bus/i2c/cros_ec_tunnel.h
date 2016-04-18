/*
 *  Copyright (C) 2016 Google, Inc
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 * Expose an I2C passthrough to the ChromeOS EC.
 */

#ifndef __CROS_EC_TUNNEL_I2C_H__
#define __CROS_EC_TUNNEL_I2C_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/ec.h"
#include <libpayload.h>
#include <assert.h>
#include <stddef.h>

typedef struct CrosECTunnelI2c {
	I2cOps ops;
	CrosEc *ec;
	uint16_t remote_bus;

	uint8_t request_buf[256];
	uint8_t response_buf[256];
} CrosECTunnelI2c;

/* -----------------------------------------------------------------------
 * cros ec tunnel i2c init function.
 *   devidx: EC devidx (should be 0)
 *   remote_bus: tunnel bus number
 *   Returns: bus
 */
CrosECTunnelI2c *new_cros_ec_tunnel_i2c(CrosEc *ec,
					uint16_t remote_bus);

#endif

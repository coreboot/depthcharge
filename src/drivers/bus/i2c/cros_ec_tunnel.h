/*
 *  Copyright 2016 Google, Inc
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
 *   remote_bus: tunnel bus number
 *   Returns: bus
 */
CrosECTunnelI2c *new_cros_ec_tunnel_i2c(CrosEc *ec,
					uint16_t remote_bus);

/*------------------------------------------------------------------------
 * Protect all the TCPC I2C tunnels in EC
 *   ec: EC instance which helps protect the TCPC I2C tunnels
 *   Returns: < 0 on error, 0 on success
 */
int cros_ec_tunnel_i2c_protect_tcpc_ports(CrosEc *ec);

/* -----------------------------------------------------------------------
 * Protect the I2C bus, or query the protection status (exact nature of the
 * bus protection is board-specific, implemented in EC, ranging from no-op
 * to blocking all accesses on the given bus).
 */
int cros_ec_tunnel_i2c_protect(CrosECTunnelI2c *bus);
int cros_ec_tunnel_i2c_protect_status(CrosECTunnelI2c *bus, int *status);

#endif

/*
 * Copyright 2016 Google Inc.
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

#ifndef __DRIVERS_MISC_ANX7688_H__
#define __DRIVERS_MISC_ANX7688_H__

#include <stdint.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/vboot_ec.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"

typedef struct Anx7688
{
	VbootEcOps vboot;
	CrosECTunnelI2c *bus;
} Anx7688;


Anx7688 *new_anx7688(CrosECTunnelI2c *bus);

#endif /* __DRIVERS_POWER_TPS65913_H__ */

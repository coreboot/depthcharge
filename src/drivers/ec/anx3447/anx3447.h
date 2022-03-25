/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __DRIVERS_EC_ANX3447_H__
#define __DRIVERS_EC_ANX3447_H__

#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/vboot_auxfw.h"

typedef struct Anx3447
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
		uint16_t fw_rev;
	} chip;
} Anx3447;

Anx3447 *new_anx3447(CrosECTunnelI2c *bus, int ec_pd_id);

#endif /* __DRIVERS_EC_ANX3447_H__ */

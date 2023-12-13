/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __DRIVERS_EC_ANX7510_H__
#define __DRIVERS_EC_ANX7510_H__

#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/vboot_auxfw.h"

typedef struct Anx7510 {
	VbootAuxfwOps fw_ops;
	CrosECTunnelI2c *bus;
	int ec_pd_id;
	int debug_updated;
	uint8_t last_offset;
	uint8_t last_saddr;
	uint16_t (*get_hash)(struct Anx7510 *me);

	/* these are cached from chip regs */
	struct {
		uint16_t vendor;
		uint16_t device;
		uint16_t fw_rev;
		uint16_t hdcp_tx_rev;
		uint16_t hdcp_rx_rev;
		uint16_t cust_rev;
	} chip;
} Anx7510;

/* main ocm firmware update */
Anx7510 *new_anx7510_ocm(CrosECTunnelI2c *bus, int ec_pd_id);
/* customer define area update */
Anx7510 *new_anx7510_cust(CrosECTunnelI2c *bus, int ec_pd_id);
/* HDCP RX firmware update */
Anx7510 *new_anx7510_rx(CrosECTunnelI2c *bus, int ec_pd_id);
/* HDCP TX firmware update */
Anx7510 *new_anx7510_tx(CrosECTunnelI2c *bus, int ec_pd_id);

#endif /* __DRIVERS_EC_ANX7510_H__ */

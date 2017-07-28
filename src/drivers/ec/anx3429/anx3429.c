/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * MCU - microcontroller
 * OCM - on chip microcontroller
 * OTP - one time programmable (ROM)
 *	16K words of 64 bit data + 8 bit ECC
 *
 * fun fact - it's an 18MHz 8051
 */

#include <endian.h>
#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/ec/anx3429/anx3429.h"

/*
 * enable debug code.
 * level 1 adds some logging
 * level 2 forces FW update, even more logging.
 */
#define ANX3429_DEBUG	0

#if (ANX3429_DEBUG > 0)
#define debug(msg, args...)	printf("%s: " msg, __func__, ##args)
#else
#define debug(msg, args...)
#endif

static VbError_t anx3429_check_hash(const VbootAuxFwOps *vbaux,
				    const uint8_t *hash, size_t hash_size,
				    VbAuxFwUpdateSeverity_t *severity)
{
	Anx3429 *me = container_of(vbaux, Anx3429, fw_ops);

	debug("call...\n");

	if (hash_size != sizeof(me->chip.fw_rev))
		return VBERROR_INVALID_PARAMETER;

	*severity = VB_AUX_FW_NO_UPDATE;

	return VBERROR_SUCCESS;
}

static VbError_t anx3429_update_image(const VbootAuxFwOps *vbaux,
				      const uint8_t *image,
				      const size_t image_size)
{
	VbError_t status = VBERROR_UNKNOWN;

	debug("call...\n");

	return status;
}

static VbError_t anx3429_protect(const VbootAuxFwOps *vbaux)
{
	Anx3429 *me = container_of(vbaux, Anx3429, fw_ops);

	debug("call...\n");

	if (cros_ec_tunnel_i2c_protect(me->bus) != 0) {
		printf("anx3429.%d: could not protect EC I2C tunnel\n",
		       me->ec_pd_id);
		return VBERROR_UNKNOWN;
	}
	return VBERROR_SUCCESS;
}

static const VbootAuxFwOps anx3429_fw_ops = {
	.fw_image_name = "anx3429_ocm.bin",
	.fw_hash_name = "anx3429_ocm.hash",
	.check_hash = anx3429_check_hash,
	.update_image = anx3429_update_image,
	.protect = anx3429_protect,
};

Anx3429 *new_anx3429(CrosECTunnelI2c *bus, int ec_pd_id)
{
	Anx3429 *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->ec_pd_id = ec_pd_id;
	me->fw_ops = anx3429_fw_ops;

	return me;
}

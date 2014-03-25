/*
 * Copyright 2013 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <assert.h>
#include <libpayload.h>

#include "base/list.h"
#include "board/daisy/i2c_arb.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/gpio/gpio.h"

static void i2c_release_bus(GpioOps *request)
{
	request->set(request, 1);
}

static int i2c_claim_bus(GpioOps *request, GpioOps *grant)
{
	// Request the bus.
	if (request->set(request, 0) < 0)
		return 1;

	// Wait for the EC to give it to us.
	int timeout = 2000 * 100; // 2s.
	while (timeout--) {
		int value = grant->get(grant);
		if (value  < 0) {
			i2c_release_bus(request);
			return 1;
		}
		if (value == 1)
			return 0;
		udelay(10);
	}

	// Time out, recind our request.
	i2c_release_bus(request);

	return 1;
}

static int i2c_arb_transfer(struct I2cOps *me, I2cSeg *segments, int seg_count)
{
	assert(me);
	SnowI2cArb *arb = container_of(me, SnowI2cArb, ops);

	if (!arb->ready) {
		i2c_release_bus(arb->request);
		arb->ready = 1;
	}

	if (i2c_claim_bus(arb->request, arb->grant))
		return 1;
	int res = arb->bus->transfer(arb->bus, segments, seg_count);
	i2c_release_bus(arb->request);
	return res;
}

SnowI2cArb *new_snow_i2c_arb(I2cOps *bus, GpioOps *request, GpioOps *grant)
{
	SnowI2cArb *arb = xzalloc(sizeof(*arb));
	arb->ops.transfer = &i2c_arb_transfer;
	arb->bus = bus;
	arb->request = request;
	arb->grant = grant;
	return arb;
}

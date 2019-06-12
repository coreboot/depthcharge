/*
 * Chromium OS EC driver - I2C interface
 *
 * Copyright 2012 Google Inc.
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * The Matrix Keyboard Protocol driver handles talking to the keyboard
 * controller chip. Mostly this is for keyboard functions, but some other
 * things have slipped in, so we provide generic services to talk to the
 * KBC.
 */

#include <assert.h>
#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/i2c.h"
#include "drivers/ec/cros/message.h"

static void proto3_fill_out_segs(I2cSeg *segs, CrosEcI2cBus *bus,
				uint32_t out_bytes, uint32_t in_bytes)
{
	segs[0].read = 0;
	segs[0].chip = bus->chip;
	segs[0].buf = bus->buf;
	segs[0].len = out_bytes;
	segs[1].read = 1;
	segs[1].chip = bus->chip;
	segs[1].buf = bus->buf;
	segs[1].len = in_bytes;
}

static int send_packet(CrosEcBusOps *me, const void *dout, uint32_t dout_len,
		       void *din, uint32_t din_len)

{
	CrosEcI2cBus *bus = container_of(me, CrosEcI2cBus, ops);
	uint8_t *bytes;
	I2cSeg segs[2];

	/* There is one framing byte at the beginning of the sent data. */
	uint32_t out_bytes = dout_len + 1;
	/* The recieved data contains a return code and size before the data. */
	uint32_t in_bytes = din_len + 2;

	if (!bus->buf)
		bus->buf = xmalloc(MSG_BYTES);

	/*
	 * Sanity-check I/O sizes given transaction overhead in internal
	 * buffers.
	 */
	if (out_bytes > MSG_BYTES) {
		printf("%s: Cannot send %d bytes\n", __func__, dout_len);
		return -EC_RES_BUS_ERROR;
	}
	if (in_bytes > MSG_BYTES) {
		printf("%s: Cannot receive %d bytes\n", __func__, din_len);
		return -EC_RES_BUS_ERROR;
	}
	assert(dout_len >= 0);

	proto3_fill_out_segs(segs, bus, out_bytes, in_bytes);

	/*
	 * Copy command and data into output buffer so we can do a single I2C
	 * burst transaction.
	 */
	bytes = bus->buf;
	*bytes++ = EC_COMMAND_PROTOCOL_3;
	memcpy(bytes, dout, dout_len);
	bytes += dout_len;

	assert(out_bytes == bytes - (uint8_t *)bus->buf);

	/* Send output data */
	cros_ec_dump_data("out", -1, bus->buf, out_bytes);

	if (bus->bus->transfer(bus->bus, segs, ARRAY_SIZE(segs)))
		return -EC_RES_BUS_ERROR;

	cros_ec_dump_data("in", -1, bus->buf, in_bytes);

	bytes = bus->buf;

	int result = *bytes++;
	if (result != EC_RES_SUCCESS) {
		printf("%s: Received bad result code %d\n", __func__, result);
		return -result;
	}

	int len = *bytes++;
	if (len + 2 > MSG_BYTES) {
		printf("%s: Received length %#02x too large\n",
		       __func__, len);
		return -EC_RES_RESPONSE_TOO_BIG;
	}

	din_len = MIN(din_len, len);
	if (din)
		memcpy(din, bytes, din_len);

	return EC_RES_SUCCESS;
}

CrosEcI2cBus *new_cros_ec_i2c_bus(I2cOps *i2c_bus, uint8_t chip)
{
	assert(i2c_bus);

	CrosEcI2cBus *bus = xzalloc(sizeof(*bus));
	bus->ops.send_packet = &send_packet;
	bus->bus = i2c_bus;
	bus->chip = chip;

	return bus;
}

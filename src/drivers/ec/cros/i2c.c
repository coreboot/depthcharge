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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
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
#include "config.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/i2c.h"
#include "drivers/ec/cros/message.h"

static int send_command(CrosEcBusOps *me, uint8_t cmd, int cmd_version,
			const void *dout, uint32_t dout_len,
			void *din, uint32_t din_len)
{
	CrosEcI2cBus *bus = container_of(me, CrosEcI2cBus, ops);
	uint8_t *bytes;

	// Header + data + checksum.
	uint32_t out_bytes = CROS_EC_I2C_OUT_HDR_SIZE + dout_len + 1;
	uint32_t in_bytes = CROS_EC_I2C_IN_HDR_SIZE + din_len + 1;

	if (!bus->buf)
		bus->buf = xmalloc(MSG_BYTES);

	/*
	 * Sanity-check I/O sizes given transaction overhead in internal
	 * buffers.
	 */
	if (out_bytes > MSG_BYTES) {
		printf("%s: Cannot send %d bytes\n", __func__, dout_len);
		return -1;
	}
	if (in_bytes > MSG_BYTES) {
		printf("%s: Cannot receive %d bytes\n", __func__, din_len);
		return -1;
	}
	assert(dout_len >= 0);

	/*
	 * Copy command and data into output buffer so we can do a single I2C
	 * burst transaction.
	 */
	bytes = bus->buf;
	*bytes++ = EC_CMD_VERSION0 + cmd_version;
	*bytes++ = cmd;
	*bytes++ = dout_len;
	memcpy(bytes, dout, dout_len);
	bytes += dout_len;

	*bytes++ = cros_ec_calc_checksum(bus->buf,
					 CROS_EC_I2C_OUT_HDR_SIZE + dout_len);

	assert(out_bytes == bytes - (uint8_t *)bus->buf);

	/* Send output data */
	cros_ec_dump_data("out", -1, bus->buf, out_bytes);

	if (i2c_write_raw(bus->bus, bus->chip, bus->buf, out_bytes) ||
	    i2c_read_raw(bus->bus, bus->chip, bus->buf, in_bytes))
		return -1;

	bytes = bus->buf;

	int result = *bytes++;
	if (result != EC_RES_SUCCESS) {
		printf("%s: Received bad result code %d\n", __func__, result);
		return -result;
	}

	int len = *bytes++;
	if (CROS_EC_I2C_IN_HDR_SIZE + len + 1 > MSG_BYTES) {
		printf("%s: Received length %#02x too large\n",
		       __func__, len);
		return -1;
	}

	int csum = cros_ec_calc_checksum(bus->buf,
					 CROS_EC_I2C_IN_HDR_SIZE + len);
	bytes += len;

	int expected = *bytes++;
	if (csum != expected) {
		printf("%s: Invalid checksum rx %#02x, calced %#02x\n",
		       __func__, expected, csum);
		return -1;
	}
	cros_ec_dump_data("in", -1, bus->buf,
			  CROS_EC_I2C_IN_HDR_SIZE + din_len + 1);

	din_len = MIN(din_len, len);
	if (din) {
		memcpy(din, (uint8_t *)bus->buf + CROS_EC_I2C_IN_HDR_SIZE,
			din_len);
	}

	return din_len;
}

CrosEcI2cBus *new_cros_ec_i2c_bus(I2cOps *i2c_bus, uint8_t chip)
{
	assert(i2c_bus);

	CrosEcI2cBus *bus = xzalloc(sizeof(*bus));
	bus->ops.send_command = &send_command;
	bus->bus = i2c_bus;
	bus->chip = chip;

	return bus;
}

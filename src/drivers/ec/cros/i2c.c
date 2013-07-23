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

#include "base/list.h"
#include "config.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/i2c.h"

static int send_command(CrosEcBusOps *me, uint8_t cmd, int cmd_version,
			const uint8_t *dout, int dout_len,
			uint8_t **dinp, int din_len)
{
	CrosEcI2cBus *bus = container_of(me, CrosEcI2cBus, ops);

	/* version8, cmd8, arglen8, out8[dout_len], csum8 */
	int out_bytes = dout_len + 4;
	/* response8, arglen8, in8[din_len], checksum8 */
	int in_bytes = din_len + 3;
	uint8_t *ptr;
	/* Receive input data, so that args will be dword aligned */
	uint8_t *in_ptr;
	int ret;

	/*
	 * Sanity-check I/O sizes given transaction overhead in internal
	 * buffers.
	 */
	if (out_bytes > sizeof(bus->dout)) {
		printf("%s: Cannot send %d bytes\n", __func__, dout_len);
		return -1;
	}
	if (in_bytes > sizeof(bus->din)) {
		printf("%s: Cannot receive %d bytes\n", __func__, din_len);
		return -1;
	}
	assert(dout_len >= 0);
	assert(dinp);

	/*
	 * Copy command and data into output buffer so we can do a single I2C
	 * burst transaction.
	 */
	ptr = bus->dout;

	/*
	 * in_ptr starts of pointing to a dword-aligned input data buffer.
	 * We decrement it back by the number of header bytes we expect to
	 * receive, so that the first parameter of the resulting input data
	 * will be dword aligned.
	 */
	in_ptr = bus->din + sizeof(int64_t);
	*ptr++ = EC_CMD_VERSION0 + cmd_version;
	*ptr++ = cmd;
	*ptr++ = dout_len;
	in_ptr -= 2;	/* Expect status, length bytes */
	memcpy(ptr, dout, dout_len);
	ptr += dout_len;

	*ptr++ = (uint8_t)cros_ec_calc_checksum(bus->dout, dout_len + 3);

	/* Send output data */
	cros_ec_dump_data("out", -1, bus->dout, out_bytes);
	ret = bus->bus->write(bus->bus, bus->chip, 0, 0, bus->dout, out_bytes);
	if (ret)
		return -1;

	ret = bus->bus->read(bus->bus, bus->chip, 0, 0, in_ptr, in_bytes);
	if (ret)
		return -1;

	if (*in_ptr != EC_RES_SUCCESS) {
		printf("%s: Received bad result code %d\n", __func__, *in_ptr);
		return -(int)*in_ptr;
	}

	int len, csum;

	len = in_ptr[1];
	if (len + 3 > sizeof(bus->din)) {
		printf("%s: Received length %#02x too large\n",
		       __func__, len);
		return -1;
	}
	csum = cros_ec_calc_checksum(in_ptr, 2 + len);
	if (csum != in_ptr[2 + len]) {
		printf("%s: Invalid checksum rx %#02x, calced %#02x\n",
		       __func__, in_ptr[2 + din_len], csum);
		return -1;
	}
	din_len = MIN(din_len, len);
	cros_ec_dump_data("in", -1, in_ptr, din_len + 3);

	/* Return pointer to dword-aligned input data, if any */
	*dinp = bus->din + sizeof(int64_t);

	return din_len;
}

CrosEcI2cBus *new_cros_ec_i2c_bus(I2cOps *i2c_bus, uint8_t chip)
{
	assert(i2c_bus);

	CrosEcI2cBus *bus = memalign(sizeof(int64_t), sizeof(*bus));
	if (!bus) {
		printf("Failed to allocate ChromeOS EC I2C object.\n");
		return NULL;
	}
	memset(bus, 0, sizeof(*bus));
	bus->ops.send_command = &send_command;
	bus->bus = i2c_bus;
	bus->chip = chip;

	return bus;
}

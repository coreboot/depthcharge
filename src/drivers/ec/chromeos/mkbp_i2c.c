/*
 * Chromium OS mkbp driver - I2C interface
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

#include "config.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/chromeos/mkbp.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

int mkbp_bus_command(struct mkbp_dev *dev, uint8_t cmd, int cmd_version,
		     const uint8_t *dout, int dout_len,
		     uint8_t **dinp, int din_len)
{
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
	if (out_bytes > sizeof(dev->dout)) {
		printf("%s: Cannot send %d bytes\n", __func__, dout_len);
		return -1;
	}
	if (in_bytes > sizeof(dev->din)) {
		printf("%s: Cannot receive %d bytes\n", __func__, din_len);
		return -1;
	}
	assert(dout_len >= 0);
	assert(dinp);

	/*
	 * Copy command and data into output buffer so we can do a single I2C
	 * burst transaction.
	 */
	ptr = dev->dout;

	/*
	 * in_ptr starts of pointing to a dword-aligned input data buffer.
	 * We decrement it back by the number of header bytes we expect to
	 * receive, so that the first parameter of the resulting input data
	 * will be dword aligned.
	 */
	in_ptr = dev->din + sizeof(int64_t);
	if (!dev->cmd_version_is_supported) {
		/* Send an old-style command */
		*ptr++ = cmd;
		out_bytes = dout_len + 1;
		in_bytes = din_len + 2;
		in_ptr--;	/* Expect just a status byte */
	} else {
		*ptr++ = EC_CMD_VERSION0 + cmd_version;
		*ptr++ = cmd;
		*ptr++ = dout_len;
		in_ptr -= 2;	/* Expect status, length bytes */
	}
	memcpy(ptr, dout, dout_len);
	ptr += dout_len;

	if (dev->cmd_version_is_supported)
		*ptr++ = (uint8_t)mkbp_calc_checksum(dev->dout, dout_len + 3);

	/* Send output data */
	mkbp_dump_data("out", -1, dev->dout, out_bytes);
	ret = i2c_write(CONFIG_DRIVER_MKBP_I2C_BUS,
			CONFIG_DRIVER_MKBP_I2C_ADDR,
			0, 0, dev->dout, out_bytes);
	if (ret)
		return -1;

	ret = i2c_read(CONFIG_DRIVER_MKBP_I2C_BUS,
		       CONFIG_DRIVER_MKBP_I2C_ADDR,
		       0, 0, in_ptr, in_bytes);
	if (ret)
		return -1;

	if (*in_ptr != EC_RES_SUCCESS) {
		printf("%s: Received bad result code %d\n", __func__, *in_ptr);
		return -(int)*in_ptr;
	}

	if (dev->cmd_version_is_supported) {
		int len, csum;

		len = in_ptr[1];
		if (len + 3 > sizeof(dev->din)) {
			printf("%s: Received length %#02x too large\n",
			       __func__, len);
			return -1;
		}
		csum = mkbp_calc_checksum(in_ptr, 2 + len);
		if (csum != in_ptr[2 + len]) {
			printf("%s: Invalid checksum rx %#02x, calced %#02x\n",
			       __func__, in_ptr[2 + din_len], csum);
			return -1;
		}
		din_len = min(din_len, len);
		mkbp_dump_data("in", -1, in_ptr, din_len + 3);
	} else {
		mkbp_dump_data("in (old)", -1, in_ptr, in_bytes);
	}

	/* Return pointer to dword-aligned input data, if any */
	*dinp = dev->din + sizeof(int64_t);

	return din_len;
}

/**
 * Initialize I2C protocol.
 *
 * @param dev		MKBP device
 * @return 0 if ok, -1 on error
 */
int mkbp_bus_init(struct mkbp_dev *dev)
{
	dev->cmd_version_is_supported = 0;

	return 0;
}

/*
 * Chromium OS EC driver - LPC interface
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

#include <libpayload.h>

#include "base/list.h"
#include "base/time.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/timer/timer.h"

static int wait_for_sync(void)
{
	uint64_t start = timer_us(0);
	while (inb(EC_LPC_ADDR_HOST_CMD) & EC_LPC_STATUS_BUSY_MASK) {
		if (timer_us(start) > 1000 * 1000) {
			printf("%s: Timeout waiting for CrosEC sync\n",
				__func__);
			return -1;
		}
	}
	return 0;
}

static void outb_range(const uint8_t *data, uint16_t port, int size)
{
	while (size--)
		outb(*data++, port++);
}

static void inb_range(uint8_t *data, uint16_t port, int size)
{
	while (size--)
		*data++ = inb(port++);
}

static int send_command(CrosEcBusOps *me, uint8_t cmd, int cmd_version,
			const uint8_t *dout, int dout_len,
			uint8_t **dinp, int din_len)
{
	struct ec_lpc_host_args args;
	CrosEcLpcBus *bus = container_of(me, CrosEcLpcBus, ops);

	if (!bus->initialized) {
		if (cros_ec_init(me))
			return -1;
	}

	if (dout_len > EC_HOST_PARAM_SIZE) {
		printf("%s: Cannot send %d bytes\n", __func__, dout_len);
		return -1;
	}

	/* Fill in args */
	args.flags = EC_HOST_ARGS_FLAG_FROM_HOST;
	args.command_version = cmd_version;
	args.data_size = dout_len;

	/* Calculate checksum */
	args.checksum = cmd + args.flags + args.command_version +
		args.data_size + cros_ec_calc_checksum(dout, dout_len);

	if (wait_for_sync()) {
		printf("%s: Timeout waiting ready\n", __func__);
		return -1;
	}

	/* Write args */
	outb_range((uint8_t *)&args, EC_LPC_ADDR_HOST_ARGS, sizeof(args));

	/* Write data, if any */
	outb_range(dout, EC_LPC_ADDR_HOST_PARAM, dout_len);

	outb(cmd, EC_LPC_ADDR_HOST_CMD);

	if (wait_for_sync()) {
		printf("%s: Timeout waiting for response\n", __func__);
		return -1;
	}

	/* Check result */
	int res = inb(EC_LPC_ADDR_HOST_DATA);
	if (res) {
		printf("%s: CrosEC result code %d\n", __func__, res);
		return -res;
	}

	/* Read back args */
	inb_range((uint8_t *)&args, EC_LPC_ADDR_HOST_ARGS, sizeof(args));

	/*
	 * If EC didn't modify args flags, then somehow we sent a new-style
	 * command to an old EC, which means it would have read its params
	 * from the wrong place.
	 */
	if (!(args.flags & EC_HOST_ARGS_FLAG_TO_HOST)) {
		printf("%s: CrosEC protocol mismatch\n", __func__);
		return -EC_RES_INVALID_RESPONSE;
	}

	if (args.data_size > din_len) {
		printf("%s: CrosEC returned too much data %d > %d\n",
		      __func__, args.data_size, din_len);
		return -EC_RES_INVALID_RESPONSE;
	}

	/* Read data, if any */
	inb_range((uint8_t *)bus->din, EC_LPC_ADDR_HOST_PARAM, args.data_size);

	/* Verify checksum */
	uint8_t csum = cmd + args.flags + args.command_version +
		args.data_size +
		cros_ec_calc_checksum(bus->din, args.data_size);

	if (args.checksum != (uint8_t)csum) {
		printf("%s: CrosEC response has invalid checksum\n", __func__);
		return -EC_RES_INVALID_CHECKSUM;
	}
	*dinp = bus->din;

	/* Return actual amount of data received */
	return args.data_size;
}

/**
 * Initialize LPC protocol.
 *
 * @param dev		CrosEC device
 * @param blob		Device tree blob
 * @return 0 if ok, -1 on error
 */
static int cros_ec_lpc_init(CrosEcBusOps *me)
{
	int byte, i;
	CrosEcLpcBus *bus = container_of(me, CrosEcLpcBus, ops);

	if (bus->initialized)
		return 0;
	bus->initialized = 1;

	/* See if we can find an EC at the other end */
	byte = 0xff;
	byte &= inb(EC_LPC_ADDR_HOST_CMD);
	byte &= inb(EC_LPC_ADDR_HOST_DATA);
	for (i = 0; i < EC_HOST_PARAM_SIZE && (byte == 0xff); i++)
		byte &= inb(EC_LPC_ADDR_HOST_PARAM + i);
	if (byte == 0xff) {
		printf("%s: CrosEC device not found on LPC bus\n",
			__func__);
		return -1;
	}

	return 0;
}

CrosEcLpcBus *new_cros_ec_lpc_bus(void)
{
	CrosEcLpcBus *bus = memalign(sizeof(int64_t), sizeof(*bus));
	if (!bus) {
		printf("Failed to allocate CrosEC LPC object.\n");
		return NULL;
	}
	memset(bus, 0, sizeof(*bus));
	bus->ops.init = &cros_ec_lpc_init;
	bus->ops.send_command = &send_command;

	return bus;
}

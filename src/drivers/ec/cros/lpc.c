/*
 * Chromium OS EC driver - LPC interface
 *
 * Copyright 2012 Google LLC
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

#include <libpayload.h>

#include "drivers/ec/cros/lpc.h"
#include "drivers/ec/cros/lpc_mec.h"
#include "drivers/ec/cros/lpc_rtk.h"

static int wait_for_sync(CrosEcBusOps *me, uint64_t timeout_us)
{
	uint64_t start = timer_us(0);
	uint8_t data;

	while (timer_us(start) < timeout_us) {
		me->read(&data, EC_LPC_ADDR_HOST_CMD, 1);
		if (!(data & EC_LPC_STATUS_BUSY_MASK))
			return 0;
	}
	return -1;
}

static int wait_for_data_ready(CrosEcBusOps *me, uint64_t timeout_us)
{
	uint64_t start = timer_us(0);
	uint8_t data;

	if (!CONFIG(CROS_EC_ENABLE_RTK))
		return 0;

	while (timer_us(start) < timeout_us) {
		me->read(&data, EC_LPC_ADDR_HOST_CMD, 1);
		if (data & EC_LPC_CMDR_DATA)
			return 0;
	}
	return -1;
}

static void lpc_write(const uint8_t *data, uint16_t port, int size)
{
	while (size--)
		outb(*data++, port++);
}

static void lpc_read(uint8_t *data, uint16_t port, int size)
{
	while (size--)
		*data++ = inb(port++);
}

static void mec_emi_write_address(uint16_t addr, uint8_t access_mode)
{
	/* Address relative to start of EMI range */
	addr -= MEC_EMI_RANGE_START;
	outb((addr & 0xfc) | access_mode, MEC_EMI_EC_ADDRESS_B0);
	outb((addr >> 8) & 0x7f, MEC_EMI_EC_ADDRESS_B1);
}

static void mec_io_bytes(int write, uint8_t *data, uint16_t port, int size) {
	int i = 0;
	int io_addr;
	uint8_t access_mode, new_access_mode;

	if (size == 0)
		return;

	/*
	 * Long access cannot be used on misaligned data since reading B0 loads
	 * the data register and writing B3 flushes.
	 */
	if ((port & 0x3) || (size < 4))
		access_mode = ACCESS_TYPE_BYTE;
	else
		access_mode = ACCESS_TYPE_LONG_AUTO_INCREMENT;

	/* Initialize I/O at desired address */
	mec_emi_write_address(port, access_mode);

	/* Skip bytes in case of misaligned port */
	io_addr = MEC_EMI_EC_DATA_B0 + (port & 0x3);
	while (i < size) {
		while (io_addr <= MEC_EMI_EC_DATA_B3) {
			if (write)
				outb(data[i++], io_addr++);
			else
				data[i++] = inb(io_addr++);

			port++;
			/* Extra bounds check in case of misaligned length */
			if (i == size)
				return;
		}

		/*
		 * Use long auto-increment access except for misaligned write,
		 * since writing B3 triggers the flush.
		 */
		if (size - i < 4 && write)
			new_access_mode = ACCESS_TYPE_BYTE;
		else
			new_access_mode = ACCESS_TYPE_LONG_AUTO_INCREMENT;
		if (new_access_mode != access_mode ||
		    access_mode != ACCESS_TYPE_LONG_AUTO_INCREMENT) {
			access_mode = new_access_mode;
			mec_emi_write_address(port, access_mode);
		}

		/* Access [B0, B3] on each loop pass */
		io_addr = MEC_EMI_EC_DATA_B0;
	}
}

static void mec_write(const uint8_t *data, uint16_t port, int size)
{
	if (port >= MEC_EMI_RANGE_START && port <= MEC_EMI_RANGE_END)
		mec_io_bytes(1, (uint8_t *)data, port, size);
	else
		lpc_write(data, port, size);
}

static void mec_read(uint8_t *data, uint16_t port, int size)
{
	if (port >= MEC_EMI_RANGE_START && port <= MEC_EMI_RANGE_END)
		mec_io_bytes(0, data, port, size);
	else
		lpc_read(data, port, size);
}

static void rtk_write(const uint8_t *data, uint16_t port, int size)
{
	uint8_t *ptr = (uint8_t *)(RTK_SHARED_MEM_BASE + (port - RTK_EMI_RANGE_START));
	if (port >= RTK_EMI_RANGE_START && port <= RTK_EMI_RANGE_END) {
		for (int i = 0; i < size; ++i)
			ptr[i] = data[i];
	} else {
		lpc_write(data, port, size);
	}
}

static void rtk_read(uint8_t *data, uint16_t port, int size)
{
	uint8_t *ptr = (uint8_t *)(RTK_SHARED_MEM_BASE + (port - RTK_EMI_RANGE_START));
	if (port >= RTK_EMI_RANGE_START && port <= RTK_EMI_RANGE_END) {
		for (int i = 0; i < size; ++i)
			data[i] = ptr[i];
	} else {
		lpc_read(data, port, size);
	}
}

static int send_packet(CrosEcBusOps *me, const void *dout, uint32_t dout_len,
		       void *din, uint32_t din_len)
{
	uint8_t data;
	const struct ec_host_request *rq = dout;
	/*
	 * If a HELLO cmd, use a short timeout, otherwise use
	 * a max timeout for a flash erase command to complete.
	 */
	uint64_t timeout =
		((rq->command == EC_CMD_HELLO) ? 100 : CROS_EC_ERASE_TIMEOUT_MS) * 1000;

	if (dout_len > EC_LPC_HOST_PACKET_SIZE) {
		printf("%s: Cannot send %d bytes\n", __func__, dout_len);
		return -EC_RES_BUS_ERROR;
	}

	if (din_len > EC_LPC_HOST_PACKET_SIZE) {
		printf("%s: Cannot receive %d bytes\n", __func__, din_len);
		return -EC_RES_BUS_ERROR;
	}

	if (wait_for_sync(me, timeout)) {
		/* Don't spam if timed out waiting for HELLO */
		if (rq->command != EC_CMD_HELLO)
			printf("%s: Timeout waiting ready (%lld ms)\n",
				__func__, timeout / 1000);
		return -EC_RES_TIMEOUT;
	}

	/* Copy packet */
	me->write(dout, EC_LPC_ADDR_HOST_PACKET, dout_len);

	/* Start the command */
	data = EC_COMMAND_PROTOCOL_3;
	me->write(&data, EC_LPC_ADDR_HOST_CMD, 1);

	if (wait_for_sync(me, timeout)) {
		if (rq->command != EC_CMD_HELLO)
			printf("%s: Timeout waiting ready (%lld ms)\n",
				__func__, timeout / 1000);
		return -EC_RES_TIMEOUT;
	}

	if (CONFIG(CROS_EC_ENABLE_RTK) && wait_for_data_ready(me, timeout)) {
		if (rq->command != EC_CMD_HELLO)
			printf("%s: Timeout waiting data ready (%lld ms)\n",
				__func__, timeout / 1000);
		return -EC_RES_TIMEOUT;
	}

	/* Check result */
	me->read(&data, EC_LPC_ADDR_HOST_DATA, 1);
	if (data) {
		if (data != EC_RES_UNAVAILABLE)
			printf("%s: CrosEC result code %d\n", __func__, data);
		return -data;
	}

	/* Read back response packet */
	me->read(din, EC_LPC_ADDR_HOST_PACKET, din_len);

	return EC_RES_SUCCESS;
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
	int i;
	uint8_t res;

	/* See if we can find an EC at the other end */
	me->read(&res, EC_LPC_ADDR_HOST_CMD, 1);
	if (res != 0xff)
		return 0;
	me->read(&res, EC_LPC_ADDR_HOST_DATA, 1);
	if (res != 0xff)
		return 0;
	for (i = 0; i < EC_PROTO2_MAX_PARAM_SIZE && (res == 0xff); i++)
		me->read(&res, EC_LPC_ADDR_HOST_PARAM + i, 1);

	if (res == 0xff) {
		printf("%s: CrosEC device not found on LPC bus\n",
			__func__);
		return -1;
	}
	return 0;
}

CrosEcLpcBus *new_cros_ec_lpc_bus(CrosEcLpcBusVariant variant)
{
	CrosEcLpcBus *bus = xzalloc(sizeof(*bus));
	bus->ops.init = &cros_ec_lpc_init;
	bus->ops.send_packet = &send_packet;

	switch (variant) {
	case CROS_EC_LPC_BUS_GENERIC:
		bus->ops.read = lpc_read;
		bus->ops.write = lpc_write;
		break;
	case CROS_EC_LPC_BUS_MEC:
		bus->ops.read = mec_read;
		bus->ops.write = mec_write;
		break;
	case CROS_EC_LPC_BUS_RTK:
		bus->ops.read = rtk_read;
		bus->ops.write = rtk_write;
		break;
	default:
		printf("%s: Unknown LPC variant %d\n", __func__, variant);
		free(bus);
		return NULL;
	}
	return bus;
}

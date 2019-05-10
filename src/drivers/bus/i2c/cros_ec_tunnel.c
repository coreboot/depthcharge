/*
 *  Copyright (C) 2016 Google, Inc
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 * Expose an I2C passthrough to the ChromeOS EC.
 */

#include "base/container_of.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/ec/cros/commands.h"

/**
 * ec_i2c_count_message - Count bytes needed for ec_i2c_construct_message
 *
 * @segments: The i2c messages to read
 * @seg_count: The number of i2c messages.
 *
 * Returns the number of bytes the messages will take up.
 */
static int ec_i2c_count_message(I2cSeg *segments, int seg_count)
{
	int i;
	int size;

	size = sizeof(struct ec_params_i2c_passthru);
	size += seg_count * sizeof(struct ec_params_i2c_passthru_msg);
	for (i = 0; i < seg_count; i++)
		if (!segments[i].read)
			size += segments[i].len;

	return size;
}

/**
 * ec_i2c_construct_message - construct a message to go to the EC
 *
 * This function effectively stuffs the segments format of depthcharge into
 * a format that the EC understands.
 *
 * @buf: The buffer to fill.  We assume that the buffer is big enough.
 * @segments: The i2c messages to read
 * @seg_count: The number of i2c messages.
 * @bus_num: The remote bus number we want to talk to.
 *
 */
static void ec_i2c_construct_message(uint8_t *buf, I2cSeg *segments,
				    int seg_count, u16 bus_num)
{
	struct ec_params_i2c_passthru *params;
	uint8_t *out_data;
	int i;

	out_data = buf + sizeof(struct ec_params_i2c_passthru) +
		   seg_count * sizeof(struct ec_params_i2c_passthru_msg);

	params = (struct ec_params_i2c_passthru *)buf;
	params->port = bus_num;
	params->num_msgs = seg_count;
	for (i = 0; i < seg_count; i++) {
		I2cSeg *seg = &segments[i];
		struct ec_params_i2c_passthru_msg *msg = &params->msg[i];

		msg->len = seg->len;
		msg->addr_flags = seg->chip;

		if (seg->read) {
			msg->addr_flags |= EC_I2C_FLAG_READ;
		} else {
			memcpy(out_data, seg->buf, seg->len);
			out_data += seg->len;
		}
	}
}

/**
 * ec_i2c_count_response - Count bytes needed for ec_i2c_parse_response
 *
 * @segments: The i2c messages to to fill up.
 * @seg_count: The number of i2c messages expected.
 *
 * Returns the number of response bytes expeced.
 */
static int ec_i2c_count_response(I2cSeg *segments, int seg_count)
{
	int size;
	int i;

	size = sizeof(struct ec_response_i2c_passthru);
	for (i = 0; i < seg_count; i++)
		if (segments[i].read)
			size += segments[i].len;

	return size;
}

/**
 * ec_i2c_parse_response - Parse a response from the EC
 *
 * We'll take the EC's response and copy it back into segments.
 *
 * @buf: The buffer to parse.
 * @segments: The i2c messages to to fill up.
 * @seg_count: The number of i2c messages
 *
 * Returns 0 or a negative error number.
 */
static int ec_i2c_parse_response(const uint8_t *buf, I2cSeg *segments,
				 int seg_count)
{
	const struct ec_response_i2c_passthru *resp;
	const uint8_t *in_data;
	int i;

	in_data = buf + sizeof(struct ec_response_i2c_passthru);

	resp = (const struct ec_response_i2c_passthru *)buf;
	if (resp->i2c_status & EC_I2C_STATUS_TIMEOUT) {
		printf("%s: Timeout.\n", __func__);
		return -1;
	} else if (resp->i2c_status & EC_I2C_STATUS_ERROR) {
		printf("%s: Status error.\n", __func__);
		return -1;
	}

	/* Other side could send us back fewer messages, but not more */
	if (resp->num_msgs > seg_count) {
		printf("%s: Too many messages (%d > %d).\n", __func__,
		       resp->num_msgs, seg_count);
		return -1;
	}

	for (i = 0; i < seg_count; i++) {
		struct I2cSeg *seg = &segments[i];

		if (seg->read) {
			memcpy(seg->buf, in_data, seg->len);
			in_data += seg->len;
		}
	}

	return 0;
}

static int i2c_transfer(I2cOps *me, I2cSeg *segments, int seg_count)
{
	CrosECTunnelI2c *bus = container_of(me, CrosECTunnelI2c, ops);

	int request_len;
	int response_len;
	int result;

	request_len = ec_i2c_count_message(segments, seg_count);
	response_len = ec_i2c_count_response(segments, seg_count);

	if (request_len > ARRAY_SIZE(bus->request_buf) ||
	    response_len > ARRAY_SIZE(bus->response_buf)) {
		printf("%s: Request or response too large (%d/%d).\n",
		       __func__, request_len, response_len);
		return -1;
	}

	ec_i2c_construct_message(bus->request_buf, segments,
				 seg_count, bus->remote_bus);

	result = ec_command(bus->ec, EC_CMD_I2C_PASSTHRU, 0,
			    bus->request_buf, request_len,
			    bus->response_buf, response_len);
	if (result < response_len) {
		printf("%s: ec_command error %d<%d.\n", __func__,
		       result, response_len);
		return result;
	}

	result = ec_i2c_parse_response(bus->response_buf, segments, seg_count);
	if (result != 0)
		return result;

	return 0;
}

int cros_ec_tunnel_i2c_protect(CrosECTunnelI2c *bus)
{
	struct ec_params_i2c_passthru_protect params = {
		.subcmd = EC_CMD_I2C_PASSTHRU_PROTECT_ENABLE,
		.port = bus->remote_bus
	};
	int result;

	result = ec_command(bus->ec, EC_CMD_I2C_PASSTHRU_PROTECT, 0,
			    &params, sizeof(params),
			    NULL, 0);

	if (result < 0)
		return result;

	return 0;
}

int cros_ec_tunnel_i2c_protect_status(CrosECTunnelI2c *bus, int *status)
{
	struct ec_params_i2c_passthru_protect params = {
		.subcmd = EC_CMD_I2C_PASSTHRU_PROTECT_STATUS,
		.port = bus->remote_bus
	};
	struct ec_response_i2c_passthru_protect response;
	int result;

	result = ec_command(bus->ec, EC_CMD_I2C_PASSTHRU_PROTECT, 0,
			    &params, sizeof(params),
			    &response, sizeof(response));

	if (result < 0)
		return result;

	if (result < sizeof(response))
		return -1;

	*status = response.status;

	return 0;
}

int cros_ec_tunnel_i2c_protect_tcpc_ports(CrosEc *ec)
{
	struct ec_params_i2c_passthru_protect protect_p = {
		.subcmd = EC_CMD_I2C_PASSTHRU_PROTECT_ENABLE_TCPCS,
	};
	int ret;

	ret = ec_command(ec, EC_CMD_I2C_PASSTHRU_PROTECT, 0,
			 &protect_p, sizeof(protect_p), NULL, 0);
	if (ret < 0)
		return ret;
	return 0;
}

CrosECTunnelI2c *new_cros_ec_tunnel_i2c(CrosEc *ec,
					uint16_t remote_bus) {
	CrosECTunnelI2c *bus = xzalloc(sizeof(*bus));

	assert(ec);

	bus->ops.transfer = &i2c_transfer;
	bus->ec = ec;
	bus->remote_bus = remote_bus;

	return bus;
}

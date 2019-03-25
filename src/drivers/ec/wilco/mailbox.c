// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 *
 * Mailbox interface for the Wilco Embedded Controller.
 */

#include <assert.h>
#include <libpayload.h>
#include <stdint.h>
#include <stdbool.h>
#include <vboot_api.h>

#include "base/container_of.h"
#include "drivers/ec/wilco/ec.h"
#include "drivers/ec/wilco/mec.h"

/**
 * enum ec_mailbox - Mailbox interface values.
 * @EC_MAILBOX_VERSION: Version of mailbox interface.
 * @EC_MAILBOX_START_COMMAND: Command to start mailbox transaction.
 * @EC_MAILBOX_PROTO_VERSION: Version of EC protocol.
 * @EC_MAILBOX_DATA_SIZE: Normal commands have a maximum 32 bytes of data.
 * @EC_MAILBOX_DATA_SIZE_EXTENDED: Extended commands return 256 bytes of data.
 * @EC_MAILBOX_DATA_EXTRA: Number of header bytes to be counted as data.
 * @EC_MAILBOX_TIMEOUT: Command timeout long enough for flash erase.
 */
enum {
	EC_MAILBOX_VERSION = 0,
	EC_MAILBOX_START_COMMAND = 0xda,
	EC_MAILBOX_PROTO_VERSION = 3,
	EC_MAILBOX_DATA_SIZE = 32,
	EC_MAILBOX_DATA_SIZE_EXTENDED = 256,
	EC_MAILBOX_DATA_EXTRA = 2,
	EC_MAILBOX_TIMEOUT = USECS_PER_SEC * 20,
};

/**
 * enum ec_response_flags - Flags set by EC during transaction.
 * @EC_CMDR_DATA: Data ready for host to read.
 * @EC_CMCR_PENDING: Write is pending to EC.
 * @EC_CMDR_BUSY: EC is busy processing a command.
 * @EC_CMDR_CMD: Last host write was a command.
 */
enum ec_response_flags {
	EC_CMDR_DATA = 0x01,
	EC_CMDR_PENDING = 0x02,
	EC_CMDR_BUSY = 0x04,
	EC_CMDR_CMD = 0x08,
};

/**
 * struct WilcoEcRequest - Mailbox request message format.
 * @struct_version: Should be %EC_MAILBOX_PROTO_VERSION
 * @checksum: Sum of all bytes must be 0.
 * @mailbox_id: Mailbox identifier, specifies the command set.
 * @mailbox_version: Mailbox interface version %EC_MAILBOX_VERSION
 * @reserved: Set to zero.
 * @data_size: Length of request, data + last 2 bytes of the header.
 * @command: Mailbox command code, unique for each mailbox_id set.
 * @reserved_raw: Set to zero for most commands, but is used by
 *                some command types and for raw commands.
 */
typedef struct __attribute__((packed)) WilcoEcRequest {
	uint8_t struct_version;
	uint8_t checksum;
	uint16_t mailbox_id;
	uint8_t mailbox_version;
	uint8_t reserved;
	uint16_t data_size;
	uint8_t command;
	uint8_t reserved_raw;
} WilcoEcRequest;

/**
 * struct WilcEcResponse - Mailbox response message format.
 * @struct_version: Should be %EC_MAILBOX_PROTO_VERSION
 * @checksum: Sum of all bytes must be 0.
 * @result: Result code from the EC.  Non-zero indicates an error.
 * @data_size: Length of the response data buffer.
 * @reserved: Set to zero.
 * @mbox0: EC returned data at offset 0 is unused (always 0) so this byte
 *         is treated as part of the header instead of the data.
 * @data: Response data buffer.  Max size is %EC_MAILBOX_DATA_SIZE_EXTENDED.
 */
typedef struct __attribute__((packed)) WilcoEcResponse {
	uint8_t struct_version;
	uint8_t checksum;
	uint16_t result;
	uint16_t data_size;
	uint8_t reserved[2];
	uint8_t mbox0;
	uint8_t data[0];
} WilcoEcResponse;

static bool wilco_ec_response_timed_out(WilcoEc *ec)
{
	uint8_t mask = EC_CMDR_PENDING | EC_CMDR_BUSY;
	uint64_t start;

	start = timer_us(0);
	do {
		if (!(inb(ec->io_base_command) & mask))
			return false;
		mdelay(1);
	} while (timer_us(start) < EC_MAILBOX_TIMEOUT);

	printf("%s: Command timeout\n", __func__);
	return true;
}

static uint8_t wilco_ec_checksum(const void *data, size_t size)
{
	uint8_t *data_bytes = (uint8_t *)data;
	uint8_t checksum = 0;
	size_t i;

	for (i = 0; i < size; i++)
		checksum += data_bytes[i];

	return checksum;
}

static void wilco_ec_prepare(WilcoEcMessage *msg, WilcoEcRequest *rq)
{
	memset(rq, 0, sizeof(*rq));

	/* Handle messages without trimming bytes from the request */
	if (msg->request_size && msg->flags & WILCO_EC_FLAG_RAW_REQUEST) {
		rq->reserved_raw = *(u8 *)msg->request_data;
		msg->request_size--;
		memmove(msg->request_data, msg->request_data + 1,
			msg->request_size);
	}

	/* Fill in request packet */
	rq->struct_version = EC_MAILBOX_PROTO_VERSION;
	rq->mailbox_id = msg->type;
	rq->mailbox_version = EC_MAILBOX_VERSION;
	rq->data_size = msg->request_size + EC_MAILBOX_DATA_EXTRA;
	rq->command = msg->command;

	/* Checksum header and data */
	rq->checksum = wilco_ec_checksum(rq, sizeof(*rq));
	rq->checksum += wilco_ec_checksum(msg->request_data, msg->request_size);
	rq->checksum = -rq->checksum;
}

static int wilco_ec_transfer(WilcoEc *ec, WilcoEcMessage *msg,
			     WilcoEcRequest *rq)
{
	WilcoEcResponse *rs;
	uint8_t checksum, flag;
	size_t size;

	/* Write request header */
	mec_io_bytes(MEC_IO_WRITE, ec->io_base_packet, 0, (uint8_t *)rq,
		     sizeof(*rq));

	/* Write request data */
	mec_io_bytes(MEC_IO_WRITE, ec->io_base_packet, sizeof(*rq),
		     msg->request_data, msg->request_size);

	/* Start the command */
	outb(EC_MAILBOX_START_COMMAND, ec->io_base_command);

	/* For some commands (eg shutdown) the EC will not respond, that's OK */
	if (msg->flags & WILCO_EC_FLAG_NO_RESPONSE) {
		printf("%s: EC does not respond to this command\n", __func__);
		return 0;
	}

	/* Wait for it to complete */
	if (wilco_ec_response_timed_out(ec)) {
		printf("%s: response timed out\n", __func__);
		return -1;
	}

	/* Check result */
	flag = inb(ec->io_base_data);
	if (flag) {
		printf("%s: command 0x%02x result %d\n", __func__,
		       msg->command, -flag);
		return -flag;
	}

	if (msg->flags & WILCO_EC_FLAG_EXTENDED_DATA)
		size = EC_MAILBOX_DATA_SIZE_EXTENDED;
	else
		size = EC_MAILBOX_DATA_SIZE;

	/* Read back response */
	rs = ec->data_buffer;
	checksum = mec_io_bytes(MEC_IO_READ, ec->io_base_packet, 0,
				(uint8_t *)rs, sizeof(*rs) + size);
	if (checksum) {
		printf("%s: bad packet checksum %02x\n", __func__,
		       rs->checksum);
		return -1;
	}

	/* Read back response */
	msg->result = rs->result;
	if (msg->result) {
		printf("%s: bad response: 0x%02x\n", __func__, msg->result);
		return -1;
	}

	/* Check the returned data size, skipping the header */
	if (rs->data_size != size) {
		printf("%s: unexpected packet size (%u != %zu)\n", __func__,
		       rs->data_size, size);
		return -1;
	}

	/* Skip 1 response data byte unless specified */
	size = (msg->flags & WILCO_EC_FLAG_RAW_RESPONSE) ? 0 : 1;
	if ((ssize_t) rs->data_size - size < msg->response_size) {
		printf("%s: response data too short (%zd < %zd)\n",
		       __func__, (ssize_t) rs->data_size - size,
		       msg->response_size);
		return -1;
	}

	/* Ignore response data bytes as requested */
	memcpy(msg->response_data, rs->data + size, msg->response_size);

	/* Return actual amount of data received */
	return msg->response_size;
}

int wilco_ec_mailbox(WilcoEc *ec, WilcoEcMessage *msg)
{
	WilcoEcRequest *rq;
	size_t size = EC_MAILBOX_DATA_SIZE;

	/* Allocate data buffer on the first call */
	if (!ec->data_buffer || !ec->data_size) {
		ec->data_size = sizeof(WilcoEcResponse) +
			EC_MAILBOX_DATA_SIZE_EXTENDED;
		ec->data_buffer = xzalloc(ec->data_size);
	}

	if (msg->request_size > size) {
		printf("%s: provided request too large: %zu > %zu\n",
		       __func__, msg->request_size, size);
		return -1;
	}

	/* Check for extended size on response data if flag is set */
	if (msg->flags & WILCO_EC_FLAG_EXTENDED_DATA)
		size = EC_MAILBOX_DATA_SIZE_EXTENDED;

	if (msg->response_size > size) {
		printf("%s: expected response too large: %zu > %zu\n",
		       __func__, msg->response_size, size);
		return -1;
	}
	if (msg->request_size && !msg->request_data) {
		printf("%s: request data missing\n", __func__);
		return -1;
	}
	if (msg->response_size && !msg->response_data) {
		printf("%s: response data missing\n", __func__);
		return -1;
	}

	/* Prepare request packet */
	rq = ec->data_buffer;
	wilco_ec_prepare(msg, rq);

	/* Do the EC transfer */
	return wilco_ec_transfer(ec, msg, rq);
}

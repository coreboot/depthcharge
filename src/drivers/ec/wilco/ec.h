/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2019 Google LLC
 *
 * Mailbox interface for the Wilco Embedded Controller.
 */

#ifndef __DRIVERS_EC_WILCO_EC_H__
#define __DRIVERS_EC_WILCO_EC_H__

#include <stdbool.h>
#include <stdint.h>

#include "base/cleanup_funcs.h"
#include "drivers/ec/vboot_ec.h"
#include "drivers/gpio/gpio.h"

#define WILCO_EC_FLAG_NO_RESPONSE	(1 << 0) /* EC does not respond */
#define WILCO_EC_FLAG_EXTENDED_DATA	(1 << 1) /* EC returns 256 data bytes */
#define WILCO_EC_FLAG_RAW_REQUEST	(1 << 2) /* Do not trim request data */
#define WILCO_EC_FLAG_RAW_RESPONSE	(1 << 3) /* Do not trim response data */
#define WILCO_EC_FLAG_RAW		(WILCO_EC_FLAG_RAW_REQUEST | \
					 WILCO_EC_FLAG_RAW_RESPONSE)

/**
 * enum wilco_ec_msg_type - Message type to select a set of command codes.
 * @WILCO_EC_MSG_LEGACY: Legacy EC messages for standard EC behavior.
 * @WILCO_EC_MSG_PROPERTY: Get/Set/Sync EC controlled NVRAM property.
 * @WILCO_EC_MSG_TELEMETRY: Telemetry data provided by the EC.
 */
enum wilco_ec_msg_type {
	WILCO_EC_MSG_LEGACY = 0x00f0,
	WILCO_EC_MSG_PROPERTY = 0x00f2,
	WILCO_EC_MSG_TELEMETRY = 0x00f5,
};

/**
 * enum wilco_ec_result - Return codes for EC commands.
 * @WILCO_EC_RESULT_SUCCESS: Command was successful.
 * @WILCO_EC_RESULT_ACCESS_DENIED: Trust level did not allow command.
 * @WILCO_EC_RESULT_FAILED: Command was not successful.
 */
enum wilco_ec_result {
	WILCO_EC_RESULT_SUCCESS = 0,
	WILCO_EC_RESULT_ACCESS_DENIED = -4,
	WILCO_EC_RESULT_FAILED = -7,
};

/**
 * typedef struct WilcoEc - Wilco Embedded Controller handle.
 * @io_base_command: I/O port for mailbox command.
 * @io_base_data: I/O port for mailbox data.
 * @io_base_packet: I/O port for mailbox packet data.
 * @data_buffer: Buffer used for EC communication.  The same buffer
 *               is used to hold the request and the response.
 * @data_size: Size of the data buffer used for EC communication.
 * @vboot: Verified boot handlers.
 */
typedef struct WilcoEc {
	uint16_t io_base_command;
	uint16_t io_base_data;
	uint16_t io_base_packet;
	CleanupFunc cleanup;
	void *data_buffer;
	size_t data_size;
	VbootEcOps vboot;
	GpioOps lid_gpio;
} WilcoEc;

/**
 * typedef struct WilcoEcMessage - Request and response message.
 * @type: Mailbox message type.
 * @flags: Message flags.
 * @command: Mailbox command code.
 * @result: Result code from the EC.  Non-zero indicates an error.
 * @request_size: Number of bytes to send to the EC.
 * @request_data: Buffer containing the request data.
 * @response_size: Number of bytes expected from the EC.
 *                 This is 32 by default and 256 if the flag
 *                 is set for %WILCO_EC_FLAG_EXTENDED_DATA
 * @response_data: Buffer containing the response data, should be
 *                 response_size bytes and allocated by caller.
 */
typedef struct WilcoEcMessage {
	enum wilco_ec_msg_type type;
	uint8_t flags;
	uint8_t command;
	uint8_t result;
	size_t request_size;
	void *request_data;
	size_t response_size;
	void *response_data;
} WilcoEcMessage;

/**
 * wilco_ec_mailbox() - Send request to the EC and receive the response.
 * @ec: Wilco EC device.
 * @msg: Wilco EC message.
 *
 * Return: Number of bytes received or negative error code on failure.
 */
int wilco_ec_mailbox(WilcoEc *ec, WilcoEcMessage *msg);

/**
 * new_wilco_ec() - Create a new Wilco EC device.
 * @mec_emi_base: MEC EMI base address.
 * @ec_host_base: EC Host/Data base address.
 *
 * Return: pointer to new EC device or NULL on failure.
 */
WilcoEc *new_wilco_ec(uint16_t mec_emi_base, uint16_t ec_host_base);

/* COMMANDS */

int wilco_ec_reboot(WilcoEc *ec);
int wilco_ec_exit_firmware(WilcoEc *ec);
int wilco_ec_power_button(WilcoEc *ec, int enable);
GpioOps *wilco_ec_lid_switch_flag(WilcoEc *ec);

#endif

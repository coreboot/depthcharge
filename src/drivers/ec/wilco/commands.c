// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 *
 * Common exported commands for the Wilco Embedded Controller.
 */

#include <assert.h>
#include <libpayload.h>
#include <vboot_api.h>

#include "base/container_of.h"
#include "drivers/ec/wilco/ec.h"

enum {
	/* Power button */
	EC_POWER_BUTTON = 0x06,
	/* EC mode commands */
	EC_MODE = 0x88,
	/* Reboot EC */
	EC_REBOOT = 0xf2,
};

enum ec_modes {
	EC_MODE_EXIT_FIRMWARE = 0x04,
	EC_MODE_RTC_RESET = 0x05,
	EC_MODE_EXIT_FACTORY = 0x05,
};

int wilco_ec_reboot(WilcoEc *ec)
{
	WilcoEcMessage msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.flags = WILCO_EC_FLAG_NO_RESPONSE,
		.command = EC_REBOOT,
	};

	printf("EC: rebooting...\n");
	return wilco_ec_mailbox(ec, &msg);
}

int wilco_ec_exit_firmware(WilcoEc *ec)
{
	uint8_t param = EC_MODE_EXIT_FIRMWARE;
	WilcoEcMessage msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.command = EC_MODE,
		.request_data = &param,
		.request_size = sizeof(param),
	};

	printf("EC: exit firmware mode\n");
	return wilco_ec_mailbox(ec, &msg);
}

int wilco_ec_power_button(WilcoEc *ec, int enable)
{
	uint8_t param = enable;
	WilcoEcMessage msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.command = EC_POWER_BUTTON,
		.request_data = &param,
		.request_size = sizeof(param),
	};

	printf("EC: %sable power button\n", enable ? "en" : "dis");
	return wilco_ec_mailbox(ec, &msg);
}

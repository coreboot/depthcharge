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
#include "drivers/gpio/gpio.h"

enum {
	/* Read power state information */
	EC_POWER_SMI = 0x04,
	/* Power button */
	EC_POWER_BUTTON = 0x06,
	/* Inform EC of OS boot */
	EC_OS_BOOT = 0x6b,
	/* EC mode commands */
	EC_MODE = 0x88,
	/* Reboot EC immediately */
	EC_REBOOT_NOW = 0xf2,
	/* Reboot EC after host powers down */
	EC_REBOOT_AFTER_POWEROFF = 0xf6,
};

enum ec_modes {
	EC_MODE_EXIT_FIRMWARE = 0x04,
	EC_MODE_RTC_RESET = 0x05,
	EC_MODE_EXIT_FACTORY = 0x05,
};

enum ec_power_smi {
	EC_POWER_SMI_LEN = 9,		/* 9 bytes */
	EC_LID_OPEN_OFFSET = 0,		/* byte 0 */
	EC_LID_OPEN_MASK = 0x10,	/* bit 4 */
};

int wilco_ec_reboot(WilcoEc *ec)
{
	WilcoEcMessage msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.command = EC_REBOOT_NOW,
	};
	int ret;

	printf("EC: rebooting now...\n");
	if (run_cleanup_funcs(CleanupOnReboot))
		printf("%s: cleanup failed!\n", __func__);

	ret = wilco_ec_mailbox(ec, &msg);
	if (ret == WILCO_EC_RESULT_ACCESS_DENIED) {
		/*
		 * If this command is unavailable use a backup
		 * command that will reboot after power off,
		 * which is the next step that vboot will do
		 * after an EC RO reboot attempt.
		 */
		msg.command = EC_REBOOT_AFTER_POWEROFF;
		printf("EC: rebooting after shutdown...\n");
		ret = wilco_ec_mailbox(ec, &msg);
	}

	return ret;
}

int wilco_ec_exit_firmware(WilcoEc *ec)
{
	uint8_t param = EC_MODE_EXIT_FIRMWARE;
	WilcoEcMessage exit_firmware_msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.command = EC_MODE,
		.request_data = &param,
		.request_size = sizeof(param),
	};
	WilcoEcMessage os_boot_msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.command = EC_OS_BOOT,
	};

	/* Inform EC of OS boot, ignore failure */
	wilco_ec_mailbox(ec, &os_boot_msg);

	printf("EC: exit firmware mode\n");
	return wilco_ec_mailbox(ec, &exit_firmware_msg);
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

static int wilco_ec_get_lid_gpio(GpioOps *me)
{
	WilcoEc *ec = container_of(me, WilcoEc, lid_gpio);
	uint8_t ec_power_smi[EC_POWER_SMI_LEN] = {};
	WilcoEcMessage msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.command = EC_POWER_SMI,
		.response_data = &ec_power_smi,
		.response_size = EC_POWER_SMI_LEN,
	};

	/* Read LID state from the EC */
	if (wilco_ec_mailbox(ec, &msg) == EC_POWER_SMI_LEN)
		return !!(ec_power_smi[EC_LID_OPEN_OFFSET] & EC_LID_OPEN_MASK);

	/* Indicate lid is open if EC command failed */
	return 1;
}

GpioOps *wilco_ec_lid_switch_flag(WilcoEc *ec)
{
	ec->lid_gpio.get = &wilco_ec_get_lid_gpio;
	return &ec->lid_gpio;
}

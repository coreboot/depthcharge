/*
 * Copyright 2026 Google LLC
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>
#include "drivers/ec/cros/ec.h"
#include "drivers/power/chromeec.h"

static int chromeec_reset(PowerOps *me)
{
	cros_ec_reboot(EC_REBOOT_FLAG_IMMEDIATE);
	halt();
}

static int chromeec_off(PowerOps *me)
{
	cros_ec_ap_poweroff();
	halt();
}

PowerOps chromec_power_ops = {
	.reboot = &chromeec_reset,
	.power_off = &chromeec_off,
};

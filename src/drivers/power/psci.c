/*
 * Copyright 2016 Google LLC
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

#include "arch/arm/smc.h"
#include "drivers/power/psci.h"

static int psci_reset(PowerOps *me)
{
	smc(PSCI_SYSTEM_RESET, 0, 0, 0);
	halt();
}

static int psci_off(PowerOps *me)
{
	smc(PSCI_SYSTEM_OFF, 0, 0, 0);
	halt();
}

PowerOps psci_power_ops = {
	.cold_reboot = &psci_reset,
	.power_off = &psci_off,
};

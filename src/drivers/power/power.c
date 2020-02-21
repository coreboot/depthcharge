/*
 * Copyright 2013 Google Inc.
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

#include <assert.h>
#include <libpayload.h>

#include "base/cleanup_funcs.h"
#include "drivers/power/power.h"

static PowerOps *power_ops;

void power_set_ops(PowerOps *ops)
{
	die_if(power_ops, "%s: Power ops already set.\n", __func__);
	power_ops = ops;
}

void cold_reboot(void)
{
	die_if(!power_ops, "%s: No power ops set.\n", __func__);
	assert(power_ops->cold_reboot);

	run_cleanup_funcs(CleanupOnReboot);

	printf("Rebooting...\n");
	power_ops->cold_reboot(power_ops);
	delay(1);
	die("Still alive?!?");
}

void power_off(void)
{
	die_if(!power_ops, "%s: No power ops set.\n", __func__);
	assert(power_ops->power_off);

	run_cleanup_funcs(CleanupOnPowerOff);

	printf("Powering off...\n");
	power_ops->power_off(power_ops);
	delay(1);
	die("Still alive?!?");
}

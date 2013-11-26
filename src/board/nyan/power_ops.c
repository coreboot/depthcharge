/*
 * This file is part of the coreboot project.
 *
 * Copyright 2013 Google Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA, 02110-1301 USA
 */

#include <libpayload.h>

#include "base/cleanup_funcs.h"
#include "board/nyan/power_ops.h"

static int nyan_gpio_reboot(PowerOps *me)
{
	if (run_cleanup_funcs(CleanupOnReboot))
		return -1;

	printf("Rebooting..\n");

	NyanPowerOps *power = container_of(me, NyanPowerOps, ops);
	power->gpio->set(power->gpio, power->val);

	halt();
}

static int nyan_pass_through_power_off(PowerOps *me)
{
	NyanPowerOps *power = container_of(me, NyanPowerOps, ops);
	return power->pass_through->power_off(power->pass_through);
}

NyanPowerOps *new_nyan_power_ops(PowerOps *pass_through,
				 GpioOps *gpio, int val)
{
	NyanPowerOps *power = malloc(sizeof(*power));
	if (!power) {
		printf("Failed to allocate Nyan power op structure.\n");
		return NULL;
	}
	memset(power, 0, sizeof(*power));
	power->ops.cold_reboot = &nyan_gpio_reboot;
	power->ops.power_off = &nyan_pass_through_power_off;
	power->pass_through = pass_through;
	power->gpio = gpio;
	power->val = val;
	return power;
}

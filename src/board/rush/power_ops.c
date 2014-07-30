/*
 * Copyright 2014 Google Inc.  All rights reserved.
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

#include "base/container_of.h"
#include "board/rush/power_ops.h"

static int rush_gpio_reboot(PowerOps *me)
{
	RushPowerOps *power = container_of(me, RushPowerOps, ops);
	power->gpio->set(power->gpio, power->val);
	halt();
}

static int rush_pass_through_power_off(PowerOps *me)
{
	RushPowerOps *power = container_of(me, RushPowerOps, ops);
	return power->pass_through->power_off(power->pass_through);
}

RushPowerOps *new_rush_power_ops(PowerOps *pass_through,
				 GpioOps *gpio, int val)
{
	RushPowerOps *power = xzalloc(sizeof(*power));
	power->ops.cold_reboot = &rush_gpio_reboot;
	power->ops.power_off = &rush_pass_through_power_off;
	power->pass_through = pass_through;
	power->gpio = gpio;
	power->val = val;
	return power;
}

/*
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
#include "base/container_of.h"
#include "board/veyron_pinky/power_ops.h"

static int rk_gpio_reboot(PowerOps *me)
{
	RkPowerOps *power = container_of(me, RkPowerOps, ops);
	power->gpio->set(power->gpio, power->val);
	halt();
}

static int rk_pmic_power_off(PowerOps *me)
{
	RkPowerOps *power = container_of(me, RkPowerOps, ops);
	return power->pmic->power_off(power->pmic);
}

RkPowerOps *new_rk_power_ops(GpioOps *gpio,
				 PowerOps *pmic, int val)
{
	RkPowerOps *power = xzalloc(sizeof(*power));
	power->ops.cold_reboot = &rk_gpio_reboot;
	power->ops.power_off = &rk_pmic_power_off;
	power->gpio = gpio;
	power->pmic = pmic;
	power->val = val;
	return power;
}

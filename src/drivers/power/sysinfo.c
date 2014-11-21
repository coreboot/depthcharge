/*
 * Copyright 2014 Google Inc.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include "base/container_of.h"
#include "drivers/power/sysinfo.h"

static int gpio_reboot(PowerOps *me)
{
	SysinfoResetPowerOps *p = container_of(me, SysinfoResetPowerOps, ops);
	p->reset_gpio->set(p->reset_gpio, 1);
	while (1);	// not halt(), it can trigger a GDB entry
	return -1;
}

static int pass_through_power_off(PowerOps *me)
{
	SysinfoResetPowerOps *p = container_of(me, SysinfoResetPowerOps, ops);
	return p->power_off_ops->power_off(p->power_off_ops);
}

SysinfoResetPowerOps *new_sysinfo_reset_power_ops(PowerOps *power_off_ops,
		new_gpio_from_coreboot_t new_gpio_from_coreboot)
{
	SysinfoResetPowerOps *p = xzalloc(sizeof(*p));
	p->ops.power_off = &pass_through_power_off;
	p->ops.cold_reboot = &gpio_reboot;
	p->power_off_ops = power_off_ops;
	p->reset_gpio = sysinfo_lookup_gpio("reset", 1, new_gpio_from_coreboot);
	die_if(!p->reset_gpio, "could not find 'reset' GPIO in coreboot table");
	return p;
}

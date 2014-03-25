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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/power/as3722.h"

enum {
	AS3722_RESET_CONTROL = 0x36
};

enum {
	AS3722_RESET_CONTROL_FORCE_RESET = 0x1 << 0,
	AS3722_RESET_CONTROL_POWER_OFF = 0x1 << 1
};

static int as3722_set_bit(I2cOps *bus, uint8_t chip, uint8_t reg, uint8_t bit)
{
	uint8_t val;
	if (i2c_readb(bus, chip, reg, &val) ||
	    i2c_writeb(bus, chip, reg, val | bit))
		return -1;
	return 0;
}

static int as3722_cold_reboot(PowerOps *me)
{
	As3722Pmic *pmic = container_of(me, As3722Pmic, ops);
	as3722_set_bit(pmic->bus, pmic->chip, AS3722_RESET_CONTROL,
		       AS3722_RESET_CONTROL_FORCE_RESET);
	halt();
}

static int as3722_power_off(PowerOps *me)
{
	As3722Pmic *pmic = container_of(me, As3722Pmic, ops);
	as3722_set_bit(pmic->bus, pmic->chip, AS3722_RESET_CONTROL,
		       AS3722_RESET_CONTROL_POWER_OFF);
	halt();
}

As3722Pmic *new_as3722_pmic(I2cOps *bus, uint8_t chip)
{
	As3722Pmic *pmic = xzalloc(sizeof(*pmic));
	pmic->ops.cold_reboot = &as3722_cold_reboot;
	pmic->ops.power_off = &as3722_power_off;
	pmic->bus = bus;
	pmic->chip = chip;
	return pmic;
}

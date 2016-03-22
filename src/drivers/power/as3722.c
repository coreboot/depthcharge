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

static int as3722_set_reg(As3722Pmic *pmic, uint8_t reg, uint8_t value)
{
	return i2c_writeb(pmic->bus, pmic->chip, reg, value);
}

As3722Pmic *new_as3722_pmic(I2cOps *bus, uint8_t chip)
{
	As3722Pmic *pmic = xzalloc(sizeof(*pmic));
	pmic->ops.cold_reboot = &as3722_cold_reboot;
	pmic->ops.power_off = &as3722_power_off;
	pmic->set_reg = as3722_set_reg;
	pmic->bus = bus;
	pmic->chip = chip;
	return pmic;
}

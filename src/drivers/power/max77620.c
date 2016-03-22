/*
 * Copyright 2015 Google Inc.
 * Portions Copyright (C) 2014 NVIDIA Corporation
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
#include "drivers/power/max77620.h"

enum {
	MAX77620_ONOFF_CFG = 0x41,
};

enum {
	MAX77620_ONOFF_CFG_SFT_RST = 0x1 << 7,
	MAX77620_ONOFF_CFG_PWR_OFF = 0x1 << 1,
};

static int max77620_set_bit(Max77620Pmic *pmic, uint8_t reg, uint8_t bit)
{
	uint8_t val;
	if (i2c_readb(pmic->bus, pmic->chip, reg, &val) ||
	    i2c_writeb(pmic->bus, pmic->chip, reg, val | bit))
		return -1;
	return 0;
}

static int max77620_set_reg(Max77620Pmic *pmic, uint8_t reg, uint8_t value)
{
	return i2c_writeb(pmic->bus, pmic->chip, reg, value);
}

static int max77620_cold_reboot(PowerOps *me)
{
	Max77620Pmic *pmic = container_of(me, Max77620Pmic, ops);
	max77620_set_bit(pmic, MAX77620_ONOFF_CFG, MAX77620_ONOFF_CFG_SFT_RST);
	halt();
}

static int max77620_power_off(PowerOps *me)
{
	Max77620Pmic *pmic = container_of(me, Max77620Pmic, ops);
	max77620_set_reg(pmic, MAX77620_ONOFF_CFG, MAX77620_ONOFF_CFG_PWR_OFF);
	halt();
}

Max77620Pmic *new_max77620_pmic(I2cOps *bus, uint8_t chip)
{
	Max77620Pmic *pmic = xzalloc(sizeof(*pmic));
	pmic->ops.cold_reboot = &max77620_cold_reboot;
	pmic->ops.power_off = &max77620_power_off;
	pmic->set_reg = max77620_set_reg;
	pmic->bus = bus;
	pmic->chip = chip;
	return pmic;
}

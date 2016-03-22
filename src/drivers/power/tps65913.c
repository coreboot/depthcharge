/*
 * Copyright 2014 Google Inc.
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
#include "drivers/power/tps65913.h"

enum {
	TPS65913_DEV_CTRL = 0xA0,
};

enum {
	TPS65913_DEV_CTRL_SW_RST = 0x1 << 1,
	TPS65913_DEV_CTRL_DEV_ON = 0x1 << 0,
};

static int tps65913_set_bit(Tps65913Pmic *pmic, uint8_t reg, uint8_t bit)
{
	uint8_t val;
	if (i2c_readb(pmic->bus, pmic->chip, reg, &val) ||
	    i2c_writeb(pmic->bus, pmic->chip, reg, val | bit))
		return -1;
	return 0;
}

static int tps65913_set_reg(Tps65913Pmic *pmic, uint8_t reg, uint8_t value)
{
	return i2c_writeb(pmic->bus, pmic->chip, reg, value);
}

static int tps65913_cold_reboot(PowerOps *me)
{
	Tps65913Pmic *pmic = container_of(me, Tps65913Pmic, ops);
	tps65913_set_bit(pmic, TPS65913_DEV_CTRL, TPS65913_DEV_CTRL_SW_RST);
	halt();
}

static int tps65913_power_off(PowerOps *me)
{
	Tps65913Pmic *pmic = container_of(me, Tps65913Pmic, ops);
	tps65913_set_reg(pmic, TPS65913_DEV_CTRL, 0);
	halt();
}

Tps65913Pmic *new_tps65913_pmic(I2cOps *bus, uint8_t chip)
{
	Tps65913Pmic *pmic = xzalloc(sizeof(*pmic));
	pmic->ops.cold_reboot = &tps65913_cold_reboot;
	pmic->ops.power_off = &tps65913_power_off;
	pmic->set_reg = tps65913_set_reg;
	pmic->bus = bus;
	pmic->chip = chip;
	return pmic;
}

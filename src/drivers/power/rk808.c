/*
 * Copyright 2014 Rockchip Electronics Co., Ltd.
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
#include "drivers/power/rk808.h"

#define RK808_DEVCTRL_REG 0x4b
#define DEV_OFF_RST       (1<<3)

static int rk808_set_bit(Rk808Pmic *pmic, uint8_t reg, uint8_t bit)
{
	uint8_t val;
	if (i2c_readb(pmic->bus, pmic->chip, reg, &val) ||
	    i2c_writeb(pmic->bus, pmic->chip, reg, val | bit))
		return -1;
	return 0;
}

static int rk808_power_off(PowerOps *me)
{
	Rk808Pmic *pmic = container_of(me, Rk808Pmic, ops);
	rk808_set_bit(pmic, RK808_DEVCTRL_REG, DEV_OFF_RST);
	halt();
	return 0;
}

Rk808Pmic *new_rk808_pmic(I2cOps *bus, uint8_t chip)
{
	Rk808Pmic *pmic = xzalloc(sizeof(*pmic));
	pmic->ops.cold_reboot = NULL;
	pmic->ops.power_off = &rk808_power_off;
	pmic->bus = bus;
	pmic->chip = chip;
	return pmic;
}

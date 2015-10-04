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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/power/max77620.h"

enum {
	MAX77620_ONOFF_CFG = 0x41,
	MAX77620_ONOFF_CFG2 = 0x42,
};

enum {
	MAX77620_ONOFF_CFG_SFT_RST = 0x1 << 7,
	MAX77620_ONOFF_CFG_PWR_OFF = 0x1 << 1,

	MAX77620_ONOFF_CFG2_SFT_RST_WK = 0x1 << 7,
};

static int max77620_clear_bit(Max77620Pmic *pmic, uint8_t reg, uint8_t bit)
{
	uint8_t val;
	if (i2c_readb(pmic->bus, pmic->chip, reg, &val) ||
	    i2c_writeb(pmic->bus, pmic->chip, reg, (val & ~bit)))
		return -1;
	return 0;
}

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
	max77620_set_bit(pmic, MAX77620_ONOFF_CFG2,
			 MAX77620_ONOFF_CFG2_SFT_RST_WK);
	max77620_set_bit(pmic, MAX77620_ONOFF_CFG, MAX77620_ONOFF_CFG_SFT_RST);
	halt();
}

static int max77620_power_off(PowerOps *me)
{
	Max77620Pmic *pmic = container_of(me, Max77620Pmic, ops);
	max77620_clear_bit(pmic, MAX77620_ONOFF_CFG2,
			   MAX77620_ONOFF_CFG2_SFT_RST_WK);
	max77620_set_reg(pmic, MAX77620_ONOFF_CFG, MAX77620_ONOFF_CFG_SFT_RST);
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

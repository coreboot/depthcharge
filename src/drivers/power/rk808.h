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

#ifndef __DRIVERS_POWER_RK808_H__
#define __DRIVERS_POWER_RK808_H__

#include <stdint.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/power/power.h"

typedef struct Rk808Power {
	PowerOps ops;
	I2cOps *bus;
	uint8_t chip;
} Rk808Pmic;

Rk808Pmic *new_rk808_pmic(I2cOps *bus, uint8_t chip);

#endif				/* __DRIVERS_POWER_RK808_H__ */

/*
 * Copyright 2013 Google LLC
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

#ifndef __DRIVERS_GPIO_PCH_H__
#define __DRIVERS_GPIO_PCH_H__

#include "drivers/gpio/gpio.h"

typedef struct LpPchGpio
{
	GpioOps ops;
	int (*use)(struct LpPchGpio *, unsigned use);
	int num;
	int dir_set;
} LpPchGpio;

LpPchGpio *new_lp_pch_gpio(unsigned num);
LpPchGpio *new_lp_pch_gpio_input(unsigned num);
LpPchGpio *new_lp_pch_gpio_output(unsigned num);
GpioOps *new_lp_pch_gpio_input_from_coreboot(uint32_t port);

#endif /* __DRIVERS_GPIO_GPIO_H__ */

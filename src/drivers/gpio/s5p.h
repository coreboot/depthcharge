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

#ifndef __DRIVERS_GPIO_S5P_H__
#define __DRIVERS_GPIO_S5P_H__

#include "drivers/gpio/gpio.h"

enum {
	GPIO_A,
	GPIO_B,
	GPIO_C,
	GPIO_D,
	GPIO_E,
	GPIO_F,
	GPIO_G,
	GPIO_H,
	GPIO_V,
	GPIO_X,
	GPIO_Y,
	GPIO_Z
};

typedef struct S5pGpio
{
	GpioOps ops;
	int (*use)(struct S5pGpio *me, unsigned use);
	int (*pud)(struct S5pGpio *me, unsigned value);
	int dir_set;
	unsigned group;
	unsigned bank;
	unsigned bit;
} S5pGpio;

S5pGpio *new_s5p_gpio(unsigned group, unsigned bank, unsigned bit);
S5pGpio *new_s5p_gpio_input(unsigned group, unsigned bank, unsigned bit);
S5pGpio *new_s5p_gpio_output(unsigned group, unsigned bank, unsigned bit);

#endif /* __DRIVERS_GPIO_S5P_H__ */

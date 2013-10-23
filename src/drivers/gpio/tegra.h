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

#ifndef __DRIVERS_GPIO_TEGRA_H__
#define __DRIVERS_GPIO_TEGRA_H__

#include "drivers/gpio/gpio.h"

typedef enum {
	GPIO_A,
	GPIO_B,
	GPIO_C,
	GPIO_D,
	GPIO_E,
	GPIO_F,
	GPIO_G,
	GPIO_H,
	GPIO_I,
	GPIO_J,
	GPIO_K,
	GPIO_L,
	GPIO_M,
	GPIO_N,
	GPIO_O,
	GPIO_P,
	GPIO_Q,
	GPIO_R,
	GPIO_S,
	GPIO_T,
	GPIO_U,
	GPIO_V,
	GPIO_W,
	GPIO_X,
	GPIO_Y,
	GPIO_Z,
	GPIO_AA,
	GPIO_BB,
	GPIO_CC,
	GPIO_DD,
	GPIO_EE,
	GPIO_FF,
	GPIO_NUM_PORTS
} TegraGpioPort;

typedef struct TegraGpio
{
	GpioOps ops;
	TegraGpioPort port;
	unsigned index;
} TegraGpio;

// We assume these GPIOs are already set up and just read/write their values.
TegraGpio *new_tegra_gpio_input(TegraGpioPort port, unsigned index);
TegraGpio *new_tegra_gpio_output(TegraGpioPort port, unsigned index);

#endif /* __DRIVERS_GPIO_TEGRA_H__ */

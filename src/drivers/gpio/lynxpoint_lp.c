/*
 * Copyright 2012 Google Inc.
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

#include <assert.h>
#include <libpayload.h>
#include <pci.h>
#include <stdint.h>

#include "base/container_of.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/lynxpoint_lp.h"

#define LP_GPIO_CONF0(num)         (0x100 + ((num) * 8))
#define  LP_GPIO_CONF0_MODE_BIT    0
#define  LP_GPIO_CONF0_DIR_BIT     2
#define  LP_GPIO_CONF0_GPI_BIT     30
#define  LP_GPIO_CONF0_GPO_BIT     31

/* Functions for manipulating GPIO regs. */

static uint32_t pch_gpiobase(void)
{
	static const uint32_t dev = PCI_DEV(0, 0x1f, 0);
	static const uint8_t pci_cfg_gpiobase = 0x48;

	static uint32_t base = ~(uint32_t)0;
	if (base != ~(uint32_t)0)
		return base;

	base = pci_read_config32(dev, pci_cfg_gpiobase);
	// Drop the IO space bit.
	base &= ~0x1;
	return base;
}

static int pch_gpio_set(unsigned num, int bit, int val)
{
	uint16_t addr = pch_gpiobase() + LP_GPIO_CONF0(num);
	uint32_t conf0 = inl(addr);
	if (val)
		conf0 |= (1 << bit);
	else
		conf0 &= ~(1 << bit);
	outl(addr, conf0);
	return 0;
}

static int pch_gpio_get(unsigned num, int bit)
{
	uint16_t addr = pch_gpiobase() + LP_GPIO_CONF0(num);
	uint32_t conf0 = inl(addr);
	return !!(conf0 & (1 << bit));
}


/* Interface functions for manipulating a GPIO. */

static int lp_pch_gpio_get_value(GpioOps *me)
{
	assert(me);
	LpPchGpio *gpio = container_of(me, LpPchGpio, ops);
	if (!gpio->dir_set) {
		if (pch_gpio_set(gpio->num, LP_GPIO_CONF0_DIR_BIT, 1) < 0)
			return -1;
		gpio->dir_set = 1;
	}
	return pch_gpio_get(gpio->num, LP_GPIO_CONF0_GPI_BIT);
}

static int lp_pch_gpio_set_value(GpioOps *me, unsigned value)
{
	assert(me);
	LpPchGpio *gpio = container_of(me, LpPchGpio, ops);
	if (!gpio->dir_set) {
		if (pch_gpio_set(gpio->num, LP_GPIO_CONF0_DIR_BIT, 0) < 0)
			return -1;
		gpio->dir_set = 1;
	}
	return pch_gpio_set(gpio->num, LP_GPIO_CONF0_GPO_BIT, value);
}

static int lp_pch_gpio_use(LpPchGpio *me, unsigned use)
{
	assert(me);
	return pch_gpio_set(me->num, LP_GPIO_CONF0_MODE_BIT, use);
}


/* Functions to allocate and set up a GPIO structure. */

LpPchGpio *new_lp_pch_gpio(unsigned num)
{
	LpPchGpio *gpio = xzalloc(sizeof(*gpio));
	gpio->use = &lp_pch_gpio_use;
	gpio->num = num;
	return gpio;
}

LpPchGpio *new_lp_pch_gpio_input(unsigned num)
{
	LpPchGpio *gpio = new_lp_pch_gpio(num);
	gpio->ops.get = &lp_pch_gpio_get_value;
	return gpio;
}

LpPchGpio *new_lp_pch_gpio_output(unsigned num)
{
	LpPchGpio *gpio = new_lp_pch_gpio(num);
	gpio->ops.set = &lp_pch_gpio_set_value;
	return gpio;
}

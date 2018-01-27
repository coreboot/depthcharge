/*
 * Copyright (C) 2015 Google Inc.
 * Copyright (C) 2017 Advanced Micro Devices, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <libpayload.h>
#include <stdint.h>

#include "base/container_of.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/kern.h"

#define KERN_PM_MMIO 0xfed80000

#define FCH_NUM_GPIOS		149
#define FCH_GPIO_REG(num)	((num) * 4)
#define  FCH_GPIO_OUTPUT_EN	23
#define  FCH_GPIO_OUTPUT_VAL	22
#define  FCH_GPIO_INPUT_VAL	16

/* Functions for manipulating GPIO regs. */

static uintptr_t fch_gpiobase(void)
{
	return KERN_PM_MMIO + 0x1500;
}

static uintptr_t fch_iomuxbase(void)
{
	return KERN_PM_MMIO + 0xd00;
}

/* Careful! MUX setting for GPIOn is not consistent for all pins.  See BKDG. */
static void fch_iomux_set(unsigned num, int val)
{
	uint8_t *addr = phys_to_virt(fch_iomuxbase() + num);

	*addr = 0x3 & (uint8_t)val;
}

static void fch_gpio_set(unsigned num, int bit, int val)
{
	uint32_t *addr = phys_to_virt(fch_gpiobase() + FCH_GPIO_REG(num));
	uint32_t conf = *addr;
	if (val)
		conf |= (1 << bit);
	else
		conf &= ~(1 << bit);
	*addr = conf;
}

static int fch_gpio_get(unsigned num, int bit)
{
	uint32_t *addr = phys_to_virt(fch_gpiobase() + FCH_GPIO_REG(num));
	uint32_t conf = *addr;
	return !!(conf & (1 << bit));
}

/* Interface functions for manipulating a GPIO. */

static int kern_fch_gpio_get_value(GpioOps *me)
{
	assert(me);
	KernGpio *gpio = container_of(me, KernGpio, ops);
	if (!gpio->dir_set) {
		/* Unnecessary but disable output so we can trust input */
		fch_gpio_set(gpio->num, FCH_GPIO_OUTPUT_EN, 0);
		gpio->dir_set = 1;
	}
	return fch_gpio_get(gpio->num, FCH_GPIO_INPUT_VAL);
}

static int kern_fch_gpio_set_value(GpioOps *me, unsigned value)
{
	assert(me);
	KernGpio *gpio = container_of(me, KernGpio, ops);
	if (!gpio->dir_set) {
		fch_gpio_set(gpio->num, FCH_GPIO_OUTPUT_EN, 1);
		gpio->dir_set = 1;
	}
	fch_gpio_set(gpio->num, FCH_GPIO_OUTPUT_VAL, value);

	return 0;
}

static int kern_fch_gpio_use(KernGpio *me, unsigned use)
{
	assert(me);
	fch_iomux_set(me->num, use);

	return 0;
}

/* Functions to allocate and set up a GPIO structure. */

KernGpio *new_kern_fch_gpio(unsigned num)
{
	assert(num < FCH_NUM_GPIOS);

	KernGpio *gpio = xzalloc(sizeof(*gpio));
	gpio->use = &kern_fch_gpio_use;
	gpio->num = num;
	return gpio;
}

KernGpio *new_kern_fch_gpio_input(unsigned num)
{
	KernGpio *gpio = new_kern_fch_gpio(num);
	gpio->ops.get = &kern_fch_gpio_get_value;
	return gpio;
}

KernGpio *new_kern_fch_gpio_output(unsigned num)
{
	KernGpio *gpio = new_kern_fch_gpio(num);
	gpio->ops.set = &kern_fch_gpio_set_value;
	return gpio;
}

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
#define FCH_GPIO_BASE		(KERN_PM_MMIO + 0x1500)
#define FCH_IOMUX_BASE		(KERN_PM_MMIO + 0xd00)

#define FCH_NUM_GPIOS		149

#define FCH_GPIO_REG(num)	(FCH_GPIO_BASE + (num) * 4)
#define  FCH_GPIO_OUTPUT_EN	23
#define  FCH_GPIO_OUTPUT_VAL	22
#define  FCH_GPIO_INPUT_VAL	16
#define FCH_IOMUX_REG(num)	(FCH_IOMUX_BASE + (num))

/* Functions for manipulating GPIO regs. */

/* Careful! MUX setting for GPIOn is not consistent for all pins.  See BKDG. */
static void fch_iomux_set(KernGpio *gpio, int val)
{
	write8(gpio->iomux, val & 3);
}

static void fch_gpio_set(KernGpio *gpio, int bit, int val)
{
	uint32_t conf = read32(gpio->reg);
	if (val)
		conf |= (1 << bit);
	else
		conf &= ~(1 << bit);
	write32(gpio->reg, conf);
}

static int fch_gpio_get(KernGpio *gpio, int bit)
{
	return !!(read32(gpio->reg) & (1 << bit));
}

/* Interface functions for manipulating a GPIO. */

static int kern_fch_gpio_get_value(GpioOps *me)
{
	assert(me);
	KernGpio *gpio = container_of(me, KernGpio, ops);
	return fch_gpio_get(gpio, FCH_GPIO_INPUT_VAL);
}

static int kern_fch_gpio_set_value(GpioOps *me, unsigned value)
{
	assert(me);
	KernGpio *gpio = container_of(me, KernGpio, ops);

	fch_gpio_set(gpio, FCH_GPIO_OUTPUT_VAL, value);

	return 0;
}

static int kern_fch_gpio_use(KernGpio *me, unsigned use)
{
	assert(me);
	fch_iomux_set(me, use);

	return 0;
}

/* Functions to allocate and set up a GPIO structure. */

KernGpio *new_kern_fch_gpio(unsigned num)
{
	assert(num < FCH_NUM_GPIOS);

	KernGpio *gpio = xzalloc(sizeof(*gpio));
	gpio->use = &kern_fch_gpio_use;
	gpio->reg = (uint32_t *)(uintptr_t)FCH_GPIO_REG(num);
	gpio->iomux = (uint8_t *)(uintptr_t)FCH_IOMUX_REG(num);
	return gpio;
}

KernGpio *new_kern_fch_gpio_input(unsigned num)
{
	KernGpio *gpio = new_kern_fch_gpio(num);
	gpio->ops.get = &kern_fch_gpio_get_value;

	/* Unnecessary but disable output so we can trust input */
	fch_gpio_set(gpio, FCH_GPIO_OUTPUT_EN, 0);

	return gpio;
}

KernGpio *new_kern_fch_gpio_output(unsigned num, unsigned value)
{
	KernGpio *gpio = new_kern_fch_gpio(num);
	gpio->ops.set = &kern_fch_gpio_set_value;

	/* Configure GPIO as an output with initial value. */
	fch_gpio_set(gpio, FCH_GPIO_OUTPUT_VAL, value);
	fch_gpio_set(gpio, FCH_GPIO_OUTPUT_EN, 1);

	return gpio;
}

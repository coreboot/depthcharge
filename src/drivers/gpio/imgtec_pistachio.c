/*
 * This file is part of the depthcharge project.
 *
 * Copyright (C) 2014 Imagination Technologies
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
#include "base/container_of.h"
#include "drivers/gpio/imgtec_pistachio.h"

#define GPIO_CONTROL(bank)		(0x200 + ((bank) * 0x24))

#define GPIO_CTRL_BIT_EN(pin)		(GPIO_CONTROL((pin) / 16) + 0x00)
#define GPIO_CTRL_OUTPUT_EN(pin)	(GPIO_CONTROL((pin) / 16) + 0x04)
#define GPIO_CTRL_OUTPUT(pin)		(GPIO_CONTROL((pin) / 16) + 0x08)
#define GPIO_CTRL_INPUT(pin)		(GPIO_CONTROL((pin) / 16) + 0x0C)

/* Functions for manipulating GPIO regs. */
static u32 imgtec_gpiobase(void)
{
	return 0xB8101C00;
}

static inline void gpio_write(u32 addr, unsigned int pin, u32 value)
{
	write32((0x10000 | !!(value)) << (pin % 16), addr);
}

static void gpio_enable(unsigned int pin)
{
	u32 addr = imgtec_gpiobase() + GPIO_CTRL_BIT_EN(pin);
	gpio_write(addr, pin, 1);
}

static int __imgtec_gpio_get(unsigned int pin)
{
	u32 addr = imgtec_gpiobase() + GPIO_CTRL_INPUT(pin);
	u32 rdata = read32(addr);
	return !!(rdata & (1 << (pin % 16)));
}

static void __imgtec_gpio_set(unsigned int pin, unsigned int value)
{
	u32 addr = imgtec_gpiobase() + GPIO_CTRL_OUTPUT(pin);
	gpio_write(addr, pin, value);
}

static void gpio_direction_input(unsigned int pin)
{
	u32 addr = imgtec_gpiobase() + GPIO_CTRL_OUTPUT_EN(pin);
	gpio_write(addr, pin, 0);
	gpio_enable(pin);
}

static void gpio_direction_output(unsigned int pin)
{
	u32 addr = imgtec_gpiobase() + GPIO_CTRL_OUTPUT_EN(pin);
	gpio_write(addr, pin, 1);
	gpio_enable(pin);
}

/* Interface functions for manipulating a GPIO. */
static int imgtec_gpio_get_value(GpioOps *me)
{
	assert(me);
	ImgGpio *gpio = container_of(me, ImgGpio, ops);
	if (!gpio->dir_set) {
		gpio_direction_input(gpio->num);
		gpio->dir_set = 1;
	}
	return __imgtec_gpio_get(gpio->num);
}

static int imgtec_gpio_set_value(GpioOps *me, unsigned int value)
{
	assert(me);
	ImgGpio *gpio = container_of(me, ImgGpio, ops);
	if (!gpio->dir_set) {
		gpio_direction_output(gpio->num);
		gpio->dir_set = 1;
	}
	__imgtec_gpio_set(gpio->num, value);
	return 0;
}

ImgGpio *new_imgtec_gpio(unsigned num)
{
	ImgGpio *gpio = xzalloc(sizeof(*gpio));
	gpio->num = num;
	return gpio;
}

ImgGpio *new_imgtec_gpio_input(unsigned num)
{
	ImgGpio *gpio = new_imgtec_gpio(num);
	gpio->ops.get = &imgtec_gpio_get_value;
	return gpio;
}

ImgGpio *new_imgtec_gpio_output(unsigned num)
{
	ImgGpio *gpio = new_imgtec_gpio(num);
	gpio->ops.set = &imgtec_gpio_set_value;
	return gpio;
}

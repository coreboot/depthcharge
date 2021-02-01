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

#define FCH_GPIO_REG(num)	(FCH_GPIO_BASE + (num) * 4)
#define  FCH_GPIO_WAKE_STS	(1 << 29)
#define  FCH_GPIO_INTERRUPT_STS	(1 << 28)
#define  FCH_GPIO_OUTPUT_EN	(1 << 23)
#define  FCH_GPIO_OUTPUT_VAL	(1 << 22)
#define  FCH_GPIO_INPUT_VAL	(1 << 16)
#define FCH_IOMUX_REG(num)	(FCH_IOMUX_BASE + (num))

/* Functions for manipulating GPIO regs. */

/*
 * This table defines the IOMUX value required to configure a particular pin
 * as its GPIO function.
 */
#if CONFIG(DRIVER_SOC_STONEYRIDGE)

#define FCH_NUM_GPIOS 149

static const uint8_t fch_gpio_use_table[FCH_NUM_GPIOS] = {
	/*         0   1   2   3   4   5   6   7   8   9 */
	/*   0 */  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,
	/*  10 */  1,  2,  2,  1,  1,  1,  2,  2,  2,  2,
	/*  20 */  2,  1,  1,  2,  1,  1,  1,  0,  0,  0,
	/*  30 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,
	/*  40 */  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,
	/*  50 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	/*  60 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	/*  70 */  0,  0,  0,  0,  1,  1,  0,  0,  0,  0,
	/*  80 */  0,  0,  0,  0,  1,  1,  1,  1,  1,  0,
	/*  90 */  0,  1,  3,  1,  0,  0,  0,  0,  0,  0,
	/* 100 */  0,  1,  1,  0,  0,  0,  0,  0,  0,  0,
	/* 110 */  0,  0,  0,  2,  2,  1,  1,  1,  1,  2,
	/* 120 */  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,
	/* 130 */  1,  3,  2,  1,  1,  1,  1,  1,  1,  1,
	/* 140 */  1,  1,  1,  1,  1,  1,  1,  1,  1
};

#elif CONFIG(DRIVER_SOC_PICASSO)

#define FCH_NUM_GPIOS 145

static const uint8_t fch_gpio_use_table[FCH_NUM_GPIOS] = {
	/*         0   1   2   3   4   5   6   7   8   9 */
	/*   0 */  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,
	/*  10 */  0,  0,  1,  1,  1,  0,  1,  1,  1,  2,
	/*  20 */  2,  2,  2,  3,  1,  0,  1,  0,  0,  1,
	/*  30 */  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,
	/*  40 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	/*  50 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	/*  60 */  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,
	/*  70 */  0,  0,  0,  0,  2,  2,  1,  0,  0,  0,
	/*  80 */  0,  0,  0,  0,  1,  1,  1,  2,  2,  2,
	/*  90 */  2,  1,  3,  0,  0,  0,  0,  0,  0,  0,
	/* 100 */  0,  0,  0,  0,  3,  3,  3,  3,  2,  2,
	/* 110 */  0,  0,  0,  2,  2,  1,  1,  0,  0,  0,
	/* 120 */  1,  1,  0,  0,  0,  0,  0,  0,  0,  1,
	/* 130 */  1,  3,  2,  0,  0,  2,  1,  2,  1,  1,
	/* 140 */  2,  1,  2,  1,  1
};
#elif CONFIG(DRIVER_SOC_CEZANNE)

#define FCH_NUM_GPIOS 148

static const uint8_t fch_gpio_use_table[FCH_NUM_GPIOS] = {
	/*         0   1   2   3   4   5   6   7   8   9 */
	/*   0 */  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,
	/*  10 */  0,  0,  1,  0,  0,  0,  1,  1,  1,  2,
	/*  20 */  2,  2,  2,  3,  1,  0,  1,  0,  0,  1,
	/*  30 */  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,
	/*  40 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	/*  50 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	/*  60 */  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,
	/*  70 */  0,  0,  0,  0,  2,  2,  1,  0,  0,  0,
	/*  80 */  0,  0,  0,  0,  1,  1,  1,  2,  2,  2,
	/*  90 */  2,  1,  3,  0,  0,  0,  0,  0,  0,  0,
	/* 100 */  0,  0,  0,  0,  3,  3,  3,  3,  2,  2,
	/* 110 */  0,  0,  0,  2,  2,  1,  1,  0,  0,  0,
	/* 120 */  1,  1,  0,  0,  0,  0,  0,  0,  0,  2,
	/* 130 */  1,  3,  2,  0,  0,  0,  0,  0,  0,  0,
	/* 140 */  0,  0,  0,  0,  0,  1,  1,  1
};

#else
	/* Force a build error if unknown SOC */
	#error Unknown SOC - Only Stoney, Picasso & Cezanne supported
#endif

/*
 * Careful! MUX setting for GPIOn is not consistent for all pins.  See:
 *   Stoney Ridge BKDG: #55072
 *   Picasso PPR: #55570
 *   Cezanne PPR: #56569
 */
static void fch_iomux_set(KernGpio *gpio, int val)
{
	write8(gpio->iomux, val & 3);
}

static void fch_gpio_set(KernGpio *gpio, uint8_t val)
{
	uint32_t conf = gpio->save_reg | FCH_GPIO_OUTPUT_EN;

	if (val)
		conf |= FCH_GPIO_OUTPUT_VAL;
	else
		conf &= ~FCH_GPIO_OUTPUT_VAL;

	write32(gpio->reg, conf);
}

static int fch_gpio_get(KernGpio *gpio)
{
	return !!(read32(gpio->reg) & FCH_GPIO_INPUT_VAL);
}

static void fch_gpio_conf_input(KernGpio *gpio)
{
	write32(gpio->reg, read32(gpio->reg) & ~FCH_GPIO_OUTPUT_EN);
}

/* Interface functions for manipulating a GPIO. */

static int kern_fch_gpio_interrupt_status(GpioOps *me)
{
	assert(me);
	KernGpio *gpio = container_of(me, KernGpio, ops);
	uint32_t reg = read32(gpio->reg);

	if (reg & FCH_GPIO_INTERRUPT_STS) {
		/* Clear interrupt status, preserve wake status */
		reg &= ~FCH_GPIO_WAKE_STS;
		write32(gpio->reg, reg);
		return 1;
	}

	return 0;
}

static int kern_fch_gpio_get_value(GpioOps *me)
{
	assert(me);
	KernGpio *gpio = container_of(me, KernGpio, ops);
	return fch_gpio_get(gpio);
}

static int kern_fch_gpio_set_value(GpioOps *me, unsigned value)
{
	assert(me);
	KernGpio *gpio = container_of(me, KernGpio, ops);

	fch_gpio_set(gpio, value);

	return 0;
}

static int kern_fch_gpio_use(KernGpio *me, unsigned use)
{
	assert(me);
	fch_iomux_set(me, use);

	return 0;
}

static int kern_fch_gpio_cleanup(CleanupFunc *cleanup, CleanupType type)
{
	KernGpio *gpio = cleanup->data;

	write32(gpio->reg, gpio->save_reg);
	write8(gpio->iomux, gpio->save_iomux);

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

	/* Save initial GPIO state */
	gpio->save_reg = read32(gpio->reg);
	gpio->save_iomux = read8(gpio->iomux);

	/* Register callback to restore GPIO state on exit */
	gpio->cleanup.cleanup = &kern_fch_gpio_cleanup;
	gpio->cleanup.types = CleanupOnHandoff | CleanupOnLegacy;
	gpio->cleanup.data = gpio;
	list_insert_after(&gpio->cleanup.list_node, &cleanup_funcs);

	return gpio;
}

KernGpio *new_kern_fch_gpio_input(unsigned num)
{
	KernGpio *gpio = new_kern_fch_gpio(num);
	gpio->ops.get = &kern_fch_gpio_get_value;

	/* Configure pin to use its GPIO function. */
	fch_iomux_set(gpio, fch_gpio_use_table[num]);

	/* Configure GPIO as an input with pulls disabled */
	fch_gpio_conf_input(gpio);

	return gpio;
}

KernGpio *new_kern_fch_gpio_latched(unsigned int num)
{
	KernGpio *gpio = new_kern_fch_gpio(num);
	gpio->ops.get = &kern_fch_gpio_interrupt_status;

	/* Configure pin to use its GPIO function. */
	fch_iomux_set(gpio, fch_gpio_use_table[num]);

	/* Configure GPIO as an input with pulls disabled */
	fch_gpio_conf_input(gpio);

	return gpio;
}

KernGpio *new_kern_fch_gpio_output(unsigned num, unsigned value)
{
	KernGpio *gpio = new_kern_fch_gpio(num);
	gpio->ops.set = &kern_fch_gpio_set_value;

	/* Configure pin to use its GPIO function. */
	fch_iomux_set(gpio, fch_gpio_use_table[num]);

	/* Configure GPIO as an output with initial value. */
	fch_gpio_set(gpio, value);

	return gpio;
}

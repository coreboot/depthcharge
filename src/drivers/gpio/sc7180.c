/*
 * This file is part of the coreboot project.
 *
 * Copyright 2019 Qualcomm Inc.
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
#include "gpio.h"
#include "sc7180.h"

static int sc7180_gpio_irq_status(GpioOps *gpioops)
{
	assert(gpioops);
	uint32_t int_status;
	Sc7180GpioCfg *gpio_cfg = container_of(gpioops, Sc7180GpioCfg, ops);
	TlmmGpio *regs = (void *)(uintptr_t)gpio_cfg->gpio.addr;

	int_status = !!read32(&regs->intr_status);
	if (int_status)
		write32(&regs->intr_status, 0);

	return int_status;
}

static int sc7180_gpio_get(struct GpioOps *gpioops)
{
	assert(gpioops);
	Sc7180GpioCfg *gpio_cfg = container_of(gpioops, Sc7180GpioCfg, ops);
	TlmmGpio *regs = (void *)(uintptr_t)gpio_cfg->gpio.addr;

	return ((read32(&regs->in_out) >> GPIO_IO_IN_SHFT) & GPIO_IO_IN_BMSK);
}

static int sc7180_gpio_set(struct GpioOps *gpioops, unsigned int value)
{
	assert(gpioops);
	Sc7180GpioCfg *gpio_cfg = container_of(gpioops, Sc7180GpioCfg, ops);
	TlmmGpio *regs = (void *)(uintptr_t)gpio_cfg->gpio.addr;

	write32(&regs->in_out, (!!value) << GPIO_IO_OUT_SHFT);

	return 0;
}

GpioOps *new_sc7180_gpio_input_from_coreboot(uint32_t gpio)
{
	return &new_sc7180_gpio_input((Sc7180GpioSpec)gpio)->ops;
}

GpioOps *new_sc7180_gpio_output_from_coreboot(uint32_t gpio)
{
	return &new_sc7180_gpio_output((Sc7180GpioSpec)gpio)->ops;
}

GpioOps *new_sc7180_gpio_latched_from_coreboot(uint32_t gpio)
{
	return &new_sc7180_gpio_latched((Sc7180GpioSpec)gpio)->ops;
}

Sc7180GpioCfg *new_sc7180_gpio_input(Sc7180GpioSpec gpio)
{
	Sc7180GpioCfg *gpio_cfg;

	gpio_cfg = xzalloc(sizeof(Sc7180GpioCfg));
	gpio_cfg->gpio = gpio;
	gpio_cfg->ops.get = &sc7180_gpio_get;

	return gpio_cfg;
}

Sc7180GpioCfg *new_sc7180_gpio_output(Sc7180GpioSpec gpio)
{
	Sc7180GpioCfg *gpio_cfg;

	gpio_cfg = xzalloc(sizeof(Sc7180GpioCfg));
	gpio_cfg->gpio = gpio;
	gpio_cfg->ops.set = &sc7180_gpio_set;

	return gpio_cfg;
}

Sc7180GpioCfg *new_sc7180_gpio_latched(Sc7180GpioSpec gpio)
{
	Sc7180GpioCfg *gpio_cfg;

	gpio_cfg = xzalloc(sizeof(Sc7180GpioCfg));
	gpio_cfg->gpio = gpio;
	gpio_cfg->ops.get = &sc7180_gpio_irq_status;

	return gpio_cfg;
}

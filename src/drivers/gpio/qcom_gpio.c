/*
 * This file is part of the coreboot project.
 *
 * Copyright 2021 Qualcomm Inc.
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

#include <arch/io.h>
#include <assert.h>
#include <libpayload.h>

#include "base/container_of.h"
#include "gpio.h"
#include "qcom_gpio.h"

static int qcom_gpio_irq_status(GpioOps *gpioops)
{
	assert(gpioops);
	uint32_t irq_status;
	GpioCfg *gpio_cfg = container_of(gpioops, GpioCfg, ops);
	Gpio *regs = (void *)(uintptr_t)gpio_cfg->gpio.addr;

	irq_status = !!read32(&regs->intr_status);
	if (irq_status)
		write32(&regs->intr_status, 0);

	return irq_status;
}

static int qcom_gpio_get(struct GpioOps *gpioops)
{
	assert(gpioops);
	GpioCfg *gpio_cfg = container_of(gpioops, GpioCfg, ops);
	Gpio *regs = (void *)(uintptr_t)gpio_cfg->gpio.addr;

	return ((read32(&regs->in_out) >> GPIO_IO_IN_SHFT) & GPIO_IO_IN_BMSK);
}

static int qcom_gpio_set(struct GpioOps *gpioops, unsigned int value)
{
	assert(gpioops);
	GpioCfg *gpio_cfg = container_of(gpioops, GpioCfg, ops);
	Gpio *regs = (void *)(uintptr_t)gpio_cfg->gpio.addr;

	write32(&regs->in_out, (!!value) << GPIO_IO_OUT_SHFT);
	return 0;
}

GpioOps *new_gpio_input_from_coreboot(uint32_t gpio)
{
	return &new_gpio_input((GpioSpec)gpio)->ops;
}

GpioOps *new_gpio_output_from_coreboot(uint32_t gpio)
{
	return &new_gpio_output((GpioSpec)gpio)->ops;
}

GpioOps *new_gpio_latched_from_coreboot(uint32_t gpio)
{
	return &new_gpio_latched((GpioSpec)gpio)->ops;
}

GpioCfg *new_gpio_input(GpioSpec gpio)
{
	GpioCfg *gpio_cfg;

	gpio_cfg = xzalloc(sizeof(GpioCfg));
	gpio_cfg->gpio = gpio;
	gpio_cfg->ops.get = &qcom_gpio_get;

	return gpio_cfg;
}

GpioCfg *new_gpio_output(GpioSpec gpio)
{
	GpioCfg *gpio_cfg;

	gpio_cfg = xzalloc(sizeof(GpioCfg));
	gpio_cfg->gpio = gpio;
	gpio_cfg->ops.set = &qcom_gpio_set;

	return gpio_cfg;
}

GpioCfg *new_gpio_latched(GpioSpec gpio)
{
	GpioCfg *gpio_cfg;

	gpio_cfg = xzalloc(sizeof(GpioCfg));
	gpio_cfg->gpio = gpio;
	gpio_cfg->ops.get = &qcom_gpio_irq_status;

	return gpio_cfg;
}

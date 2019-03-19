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
#include "drivers/gpio/gpio.h"
#include "qcs405.h"

static int qcs405_gpio_get(struct GpioOps *me)
{
	assert(me);
	Qcs405GpioCfg *gpio_cfg = container_of(me, Qcs405GpioCfg, ops);
	TlmmGpio *regs = (void *)(uintptr_t)gpio_cfg->gpio.addr;

	return ((read32(&regs->in_out) >> GPIO_IO_IN_SHFT) &
		GPIO_IO_IN_BMSK);
}

static int qcs405_gpio_set(struct GpioOps *me, unsigned int value)
{
	assert(me);
	Qcs405GpioCfg *gpio_cfg = container_of(me, Qcs405GpioCfg, ops);
	TlmmGpio *regs = (void *)(uintptr_t)gpio_cfg->gpio.addr;

	write32(&regs->in_out, (!!value) << GPIO_IO_OUT_SHFT);
	return 0;
}

GpioOps *new_qcs405_gpio_input_from_coreboot(uint32_t gpio)
{
	return &new_qcs405_gpio_input((qcs405GpioSpec)gpio)->ops;
}

GpioOps *new_qcs405_gpio_output_from_coreboot(uint32_t gpio)
{
	return &new_qcs405_gpio_output((qcs405GpioSpec)gpio)->ops;
}

Qcs405GpioCfg *new_qcs405_gpio_input(qcs405GpioSpec gpio)
{
	Qcs405GpioCfg *gpio_cfg;

	gpio_cfg = xzalloc(sizeof(Qcs405GpioCfg));
	gpio_cfg->gpio = gpio;
	gpio_cfg->ops.get = &qcs405_gpio_get;
	return gpio_cfg;
}

Qcs405GpioCfg *new_qcs405_gpio_output(qcs405GpioSpec gpio)
{
	Qcs405GpioCfg *gpio_cfg;

	gpio_cfg = xzalloc(sizeof(Qcs405GpioCfg));
	gpio_cfg->gpio = gpio;
	gpio_cfg->ops.set = &qcs405_gpio_set;
	return gpio_cfg;
}

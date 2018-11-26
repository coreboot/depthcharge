/*
 * Copyright 2018 MediaTek Inc.
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
 */

#include <assert.h>
#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/gpio/mt8183.h"

enum {
	MAX_GPIO_REG_BITS = 32,
	MAX_EINT_REG_BITS = 32,
};

static GpioRegs *gpio_reg = (GpioRegs *)0x10005000;
static EintRegs *eint_reg = (EintRegs *)0x1000B000;

static int mt_set_gpio_out(GpioOps *me, u32 output)
{
	MtGpio *gpio = container_of(me, MtGpio, ops);
	u32 pin = gpio->pin_num;
	u32 pos, bit;
	GpioRegs *reg = gpio_reg;

	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;

	if (output == 0)
		write32(&reg->dout[pos].rst, 1L << bit);
	else
		write32(&reg->dout[pos].set, 1L << bit);

	return 0;
}

static int mt_get_gpio_in(GpioOps *me)
{
	MtGpio *gpio = container_of(me, MtGpio, ops);
	u32 pin = gpio->pin_num;
	u32 pos, bit, val;
	GpioRegs *reg = gpio_reg;

	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;

	val = read32(&reg->din[pos].val);

	return (val & (1L << bit)) != 0;
}

static MtGpio *new_mtk_gpio(u32 pin)
{
	die_if(pin > GPIO_MAX, "Bad GPIO pin number %d.\n", pin);
	MtGpio *gpio = xzalloc(sizeof(*gpio));

	gpio->pin_num = pin;

	return gpio;
}

GpioOps *new_mtk_gpio_input(u32 pin)
{
	MtGpio *gpio = new_mtk_gpio(pin);

	gpio->ops.get = &mt_get_gpio_in;
	return &gpio->ops;
}

GpioOps *new_mtk_gpio_output(u32 pin)
{
	MtGpio *gpio = new_mtk_gpio(pin);

	gpio->ops.set = &mt_set_gpio_out;
	return &gpio->ops;
}

int mt_eint_irq_status(GpioOps *me)
{
	MtGpio *gpio = container_of(me, MtGpio, ops);
	u32 pin = gpio->pin_num;
	u32 pos, bit, status;
	EintRegs *reg = eint_reg;

	assert(pin < GPIO_MAX);

	pos = pin / MAX_EINT_REG_BITS;
	bit = pin % MAX_EINT_REG_BITS;

	status = (((readl(&reg->sta[pos]) & (1L << bit)) != 0) ? 1 : 0);

	if (status)
		writel(1L << bit, &reg->ack[pos]);

	return status;
}

GpioOps *new_mtk_eint(u32 pin)
{
	MtGpio *gpio = new_mtk_gpio(pin);

	gpio->ops.get = &mt_eint_irq_status;
	return &gpio->ops;
}

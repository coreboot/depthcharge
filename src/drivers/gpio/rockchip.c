/*
 * Copyright 2014 Rockchip Electronics Co., Ltd.
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
#include <stdint.h>
#include "base/container_of.h"
#include "drivers/gpio/rockchip.h"
#include "drivers/gpio/gpio.h"

static RkGpioRegs *gpio_port[] = {
	(RkGpioRegs *)0xff750000,
	(RkGpioRegs *)0xff780000,
	(RkGpioRegs *)0xff790000,
	(RkGpioRegs *)0xff7a0000,
	(RkGpioRegs *)0xff7b0000,
	(RkGpioRegs *)0xff7c0000,
	(RkGpioRegs *)0xff7d0000,
	(RkGpioRegs *)0xff7e0000,
	(RkGpioRegs *)0xff7f0000
};

static int rk_gpio_get_value(GpioOps *me)
{
	assert(me);
	RkGpio *gpio = container_of(me, RkGpio, ops);
	return (readl(&gpio_port[gpio->gpioindex.port]->ext_porta)
		>> gpio->gpioindex.num) & 0x01;
}

static int rk_gpio_set_value(GpioOps *me, unsigned value)
{
	assert(me);
	RkGpio *gpio = container_of(me, RkGpio, ops);
	clrsetbits_le32(&gpio_port[gpio->gpioindex.port]->swporta_dr,
			1 << gpio->gpioindex.num,
			!!value << gpio->gpioindex.num);
	return 0;
}

RkGpio *new_rk_gpio_input(RkGpioSpec gpioindex)
{
	RkGpio *gpio = xzalloc(sizeof(*gpio));
	gpio->ops.get = &rk_gpio_get_value;
	gpio->gpioindex = gpioindex;
	return gpio;
}

RkGpio *new_rk_gpio_output(RkGpioSpec gpioindex)
{
	RkGpio *gpio = xzalloc(sizeof(*gpio));
	gpio->ops.set = &rk_gpio_set_value;
	gpio->gpioindex = gpioindex;
	return gpio;
}

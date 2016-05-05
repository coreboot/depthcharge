/*
 * Copyright 2016 Rockchip Electronics Co., Ltd.
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

#include "drivers/gpio/rockchip.h"
#include "drivers/gpio/gpio.h"

RkGpioRegs *gpio_port[] = {
	(RkGpioRegs *)0xff720000,
	(RkGpioRegs *)0xff730000,
	(RkGpioRegs *)0xff780000,
	(RkGpioRegs *)0xff788000,
	(RkGpioRegs *)0xff790000,
};


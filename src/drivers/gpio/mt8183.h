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

#ifndef __DRIVERS_GPIO_MT8183_H__
#define __DRIVERS_GPIO_MT8183_H__

#include <stdint.h>

#include "drivers/gpio/gpio.h"

enum {
	GPIO_MAX = 180,
};

typedef struct {
	uint32_t val;
	uint32_t set;
	uint32_t rst;
	uint32_t align;
} GpioValRegs;

typedef struct {
	GpioValRegs dir[6];
	uint8_t rsv00[160];
	GpioValRegs dout[6];
	uint8_t rsv01[160];
	GpioValRegs din[6];
} GpioRegs;

typedef struct {
	GpioOps ops;
	u32 pin_num;
} MtGpio;

typedef struct {
	uint32_t sta[16];
	uint32_t ack[16];
} EintRegs;

GpioOps *new_mtk_gpio_input(u32 pin);
GpioOps *new_mtk_gpio_output(u32 pin);
GpioOps *new_mtk_eint(u32 pin);

#endif

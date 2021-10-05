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

#ifndef __DRIVERS_MT_GPIO_H__
#define __DRIVERS_MT_GPIO_H__

#include <stdint.h>
#include "drivers/gpio/gpio.h"

typedef struct {
	uint32_t val;
	uint32_t set;
	uint32_t rst;
	uint32_t align;
} GpioValRegs;

#if CONFIG(DRIVER_GPIO_MT8173)
#include "drivers/gpio/mt8173.h"
#elif CONFIG(DRIVER_GPIO_MT8183)
#include "drivers/gpio/mt8183.h"
#elif CONFIG(DRIVER_GPIO_MT8186)
#include "drivers/gpio/mt8186.h"
#elif CONFIG(DRIVER_GPIO_MT8192)
#include "drivers/gpio/mt8192.h"
#elif CONFIG(DRIVER_GPIO_MT8195)
#include "drivers/gpio/mt8195.h"
#else
#error "Unsupported GPIO config for MediaTek"
#endif

typedef struct {
	GpioOps ops;
	u32 pin_num;
} MtGpio;

GpioOps *new_mtk_gpio_input(u32 pin);
GpioOps *new_mtk_gpio_output(u32 pin);
GpioOps *new_mtk_eint(u32 pin);

#endif /* __DRIVERS_MT_GPIO_H__ */

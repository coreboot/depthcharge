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
 */

#ifndef __ROCKCHIP_GPIO_H
#define __ROCKCHIP_GPIO_H

#include <libpayload.h>
#include "drivers/gpio/gpio.h"

#define GPIO(p, b, i) ((RkGpioSpec){.port = p, .bank = GPIO_##b, .idx = i})

enum {
	GPIO_A = 0,
	GPIO_B,
	GPIO_C,
	GPIO_D,
};

typedef struct {
	u32 swporta_dr;
	u32 swporta_ddr;
	u32 reserved0[(0x30 - 0x08) / 4];
	u32 inten;
	u32 intmask;
	u32 inttype_level;
	u32 int_polarity;
	u32 int_status;
	u32 int_rawstatus;
	u32 debounce;
	u32 porta_eoi;
	u32 ext_porta;
	u32 reserved1[(0x60 - 0x54) / 4];
	u32 ls_sync;
} RkGpioRegs;

// This structure must be kept in sync with coreboot's GPIO implementation!
typedef union {
	u32 raw;
	struct {
		u16 port;
		union {
			struct {
				u16 num:5;
				u16 :11;
			};
			struct {
				u16 idx:3;
				u16 bank:2;
				u16 :11;
			};
		};
	};
} RkGpioSpec;

typedef struct RkGpio {
	GpioOps ops;
	RkGpioSpec gpioindex;
} RkGpio;

extern RkGpioRegs *gpio_port[];

GpioOps *new_rk_gpio_input_from_coreboot(uint32_t port);
GpioOps *new_rk_gpio_output_from_coreboot(uint32_t port);
GpioOps *new_rk_gpio_latched_from_coreboot(uint32_t port);
RkGpio *new_rk_gpio_latched(RkGpioSpec gpioindex);
RkGpio *new_rk_gpio_output(RkGpioSpec gpioindex);
RkGpio *new_rk_gpio_input(RkGpioSpec gpioindex);

#endif

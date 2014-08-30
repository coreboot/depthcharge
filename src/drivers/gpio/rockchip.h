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

#ifndef __RK3288_GPIO_H
#define __RK3288_GPIO_H

#include "drivers/gpio/gpio.h"

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

RkGpio *new_rk_gpio_output(RkGpioSpec gpioindex);
RkGpio *new_rk_gpio_input(RkGpioSpec gpioindex);

#endif

/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2021, The Linux Foundation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SOC_QUALCOMM_COMMOM_GPIO_H_
#define _SOC_QUALCOMM_COMMON_GPIO_H_

#include "gpio.h"

#if CONFIG(DRIVER_GPIO_SC7180)
#include "sc7180.h"
#elif CONFIG(DRIVER_GPIO_SC7280)
#include "sc7280.h"
#endif

#define GPIO(num)	((GpioSpec){.addr = GPIO##num##_ADDR})

typedef union {
	uint32_t addr;
} GpioSpec;

typedef struct {
	uint32_t _res1;
	uint32_t in_out;
	uint32_t _res2;
	uint32_t intr_status;
} Gpio;

typedef struct {
	GpioOps ops;
	GpioSpec gpio;
} GpioCfg;

/* GPIO IO: Mask */
enum gpio_io_bmsk {
	GPIO_IO_IN_BMSK = 0x1,
};

/* GPIO IO: Shift */
enum gpio_io_shft {
	GPIO_IO_IN_SHFT,
	GPIO_IO_OUT_SHFT,
};

GpioCfg *new_gpio_input(GpioSpec gpio);
GpioCfg *new_gpio_output(GpioSpec gpio);
GpioCfg *new_gpio_latched(GpioSpec gpio);

GpioOps *new_gpio_input_from_coreboot(uint32_t gpio);
GpioOps *new_gpio_output_from_coreboot(uint32_t gpio);
GpioOps *new_gpio_latched_from_coreboot(uint32_t gpio);

#endif // _SOC_QUALCOMM_COMMON_GPIO_H_

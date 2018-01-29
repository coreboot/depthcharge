/*
 * Copyright 2013 Google Inc.
 * Copyright (C) 2016 Advanced Micro Devices, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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

#ifndef __DRIVERS_GPIO_KERN_H__
#define __DRIVERS_GPIO_KERN_H__

#include "base/cleanup_funcs.h"
#include "drivers/gpio/gpio.h"

typedef struct KernGpio
{
	GpioOps ops;
	int (*use)(struct KernGpio *, unsigned use);
	uint32_t *reg;
	uint8_t *iomux;

	/* Used to save and restore GPIO configuration */
	uint32_t save_reg;
	uint8_t save_iomux;
	CleanupFunc cleanup;
} KernGpio;

KernGpio *new_kern_gpio(unsigned num);
KernGpio *new_kern_fch_gpio_output(unsigned num, unsigned value);
KernGpio *new_kern_fch_gpio_input(unsigned num);

#endif /* __DRIVERS_GPIO_KERN_H__ */

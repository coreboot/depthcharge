/*
 * This file is part of the depthcharge project.
 *
 * Copyright (C) 2014 Imagination Technologies
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

#ifndef __DRIVERS_GPIO_IMGTEC_H__
#define __DRIVERS_GPIO_IMGTEC_H__

#include <stdint.h>
#include "drivers/gpio/gpio.h"

typedef struct ImgGpio{
	GpioOps ops;
	int num;
	int dir_set;
} ImgGpio;

ImgGpio *new_imgtec_gpio(unsigned offset);
ImgGpio *new_imgtec_gpio_input(unsigned offset);
ImgGpio *new_imgtec_gpio_output(unsigned offset);

#endif /* __DRIVERS_GPIO_GPIO_H__ */

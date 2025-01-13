/*
 * This file is part of the coreboot project.
 *
 * Copyright 2013 Google LLC
 * Copyright (C) 2015 Intel Corp.
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

#ifndef	__DRIVERS_GPIO_BRASWELL_H__
#define	__DRIVERS_GPIO_BRASWELL_H__

#include <stdint.h>

#include "drivers/gpio/gpio.h"

#define GPIO_BASE_ADDRESS		0xfed80000

#define GP_SOUTHWEST			0
#define GP_NORTH			1
#define GP_EAST				2
#define GP_SOUTHEAST			3

#define COMMUNITY_BASE(community)	(GPIO_BASE_ADDRESS + community * 0x8000)

#define MAX_FAMILY_PAD_GPIO_NO		15
#define FAMILY_PAD_REGS_OFF		0x4400
#define FAMILY_PAD_REGS_SIZE		0x400
#define GPIO_REGS_SIZE			8
#define FAMILY_NUMBER(gpio_pad)		(gpio_pad / MAX_FAMILY_PAD_GPIO_NO)
#define INTERNAL_PAD_NUM(gpio_pad)	(gpio_pad % MAX_FAMILY_PAD_GPIO_NO)

#define GPIO_OFFSET(gpio_pad)		(FAMILY_PAD_REGS_OFF \
			+ (FAMILY_PAD_REGS_SIZE * FAMILY_NUMBER(gpio_pad) \
			+ (GPIO_REGS_SIZE * INTERNAL_PAD_NUM(gpio_pad))))

#define PAD_RX_BIT			1

typedef struct GpioCfg {
	GpioOps ops;
	uint32_t *addr;
} GpioCfg;

GpioCfg *new_braswell_gpio_input(int community, int offset);

#endif /* __DRIVERS_GPIO_BRASWELL_H__*/

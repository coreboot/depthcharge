/*
 * Copyright (C) 2015 Google Inc.
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

#include <arch/io.h>
#include <libpayload.h>
#include <stdint.h>

#include "base/container_of.h"
#include "drivers/gpio/braswell.h"

static int braswell_get_gpio(struct GpioOps *me)
{
	GpioCfg	*gpio = container_of(me, GpioCfg, ops);
	return !!(read32(gpio->addr) & PAD_RX_BIT);
}

GpioCfg *new_braswell_gpio_input(int community, int offset)
{
	GpioCfg	*gpio = xzalloc(sizeof(GpioCfg));
	uintptr_t reg_addr;

	reg_addr = COMMUNITY_BASE(community);
	reg_addr += GPIO_OFFSET(offset);
	gpio->addr = (uint32_t *)reg_addr;

	gpio->ops.get = &braswell_get_gpio;
	return gpio;
}

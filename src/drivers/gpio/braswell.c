/*
 * Copyright 2013 Google Inc.
 * Copyright (C) 2015 Intel Corp.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <stdint.h>

#include "drivers/gpio/braswell.h"

static int braswell_get_gpio(struct GpioOps *me)
{
	GpioCfg	*gpio = container_of(me, GpioCfg, ops);
	return !!(read32(gpio->desc.addr) & PAD_RX_BIT);
}

GpioCfg	*new_braswell_gpio_input(int community, int offset)
{
	GpioCfg	*gpio = xzalloc(sizeof(GpioCfg));
	gpio->ops.get = braswell_get_gpio;
	gpio->ops.set = NULL;
	gpio->desc.addr = (unsigned long)
		(COMMUNITY_BASE(community) + GPIO_OFFSET(offset));
	return	gpio;
}

GpioCfg	*new_braswell_gpio_output(int community, int offset)
{
	return NULL;
}

/*
 * Copyright 2012 Google Inc.
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

#include <libpayload.h>

#include "base/init_funcs.h"
#include "drivers/gpio/gpio.h"
#include "vboot/util/flag.h"

static GpioOps *flag_gpios[FLAG_MAX_FLAG];

int flag_fetch(FlagIndex index)
{
	die_if(index < 0 || index >= FLAG_MAX_FLAG,
	       "Flag index %d larger than max %d.\n", index, FLAG_MAX_FLAG);

	GpioOps *gpio = flag_gpios[index];
	die_if(gpio == NULL, "Don't have a gpio set up for flag %d.\n", index);

	return gpio_get(gpio);
}

void flag_replace(FlagIndex index, GpioOps *gpio)
{
	die_if(index < 0 || index >= FLAG_MAX_FLAG,
	       "Flag index %d larger than max %d.\n", index, FLAG_MAX_FLAG);

	flag_gpios[index] = gpio;
}

void flag_install(FlagIndex index, GpioOps *gpio)
{
	die_if(index < 0 || index >= FLAG_MAX_FLAG,
	       "Flag index %d larger than max %d.\n", index, FLAG_MAX_FLAG);

	die_if(flag_gpios[index], "Gpio already set up for flag %d.\n", index);
	flag_gpios[index] = gpio;
}

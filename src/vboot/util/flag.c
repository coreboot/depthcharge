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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "base/init_funcs.h"
#include "config.h"
#include "drivers/gpio/gpio.h"
#include "vboot/util/flag.h"

static GpioOps *flag_gpios[FLAG_MAX_FLAG];

int flag_fetch(FlagIndex index)
{
	if (index < 0 || index >= FLAG_MAX_FLAG) {
		printf("Flag index %d larger than max %d.\n",
			index, FLAG_MAX_FLAG);
		return -1;
	}

	GpioOps *gpio = flag_gpios[index];

	if (gpio == NULL) {
		printf("Don't have a gpio set up for flag %d.\n", index);
		return -1;
	}

	return gpio->get(gpio);
}

int flag_replace(FlagIndex index, GpioOps *gpio)
{
	if (index < 0 || index >= FLAG_MAX_FLAG) {
		printf("Flag index %d larger than max %d.\n",
			index, FLAG_MAX_FLAG);
		return -1;
	}

	flag_gpios[index] = gpio;
	return 0;
}

int flag_install(FlagIndex index, GpioOps *gpio)
{
	if (index < 0 || index >= FLAG_MAX_FLAG) {
		printf("Flag index %d larger than max %d.\n",
			index, FLAG_MAX_FLAG);
		return -1;
	}

	if (flag_gpios[index]) {
		printf("Gpio already set up for flag %d.\n", index);
		return -1;
	}

	flag_gpios[index] = gpio;
	return 0;
}

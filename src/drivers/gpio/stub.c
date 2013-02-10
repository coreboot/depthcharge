/*
 * Copyright 2013 Google Inc.
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

#include "drivers/gpio/gpio.h"

int gpio_use(unsigned num, unsigned use)
{
	printf("%s(%d, %d) is unimplemented.\n", __func__, num, use);
	halt();
}

int gpio_direction(unsigned num, unsigned input)
{
	printf("%s(%d, %d) is unimplemented.\n", __func__, num, input);
	halt();
}

int gpio_get_value(unsigned num)
{
	printf("%s(%d) is unimplemented.\n", __func__, num);
	halt();
}

int gpio_set_value(unsigned num, unsigned value)
{
	printf("%s(%d, %d) is unimplemented.\n", __func__, num, value);
	halt();
}

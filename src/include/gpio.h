/*
 * Copyright (c) 2012 The Chromium OS Authors.
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

#include <libpayload-config.h>
#include <libpayload.h>

#ifndef __GPIO_H__
#define __GPIO_H__

enum gpio_index {
	GPIO_WPSW = 0,
	GPIO_RECSW,
	GPIO_DEVSW,
	GPIO_LIDSW,
	GPIO_PWRSW,

	GPIO_MAX_GPIO
};

enum gpio_polarity {
	GPIO_ACTIVE_LOW = 0,
	GPIO_ACTIVE_HIGH = 1
};

typedef struct {
	enum gpio_index index;
	int port;
	int polarity;
	int value;
} gpio_t;


int gpio_fetch(enum gpio_index index, gpio_t *gpio);
int gpio_dump(gpio_t *gpio);

#endif

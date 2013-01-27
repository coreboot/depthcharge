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

#include <libpayload.h>

#include "config.h"
#include "drivers/gpio/pch.h"
#include "drivers/gpio/sysinfo.h"
#include "vboot/util/flag.h"

const char const *cb_gpio_name[FLAG_MAX_FLAG] = {
	[FLAG_WPSW] = "write protect",
	[FLAG_RECSW] = "recovery",
	[FLAG_DEVSW] = "developer",
	[FLAG_LIDSW] = "lid",
	[FLAG_PWRSW] = "power",
	[FLAG_OPROM] = "oprom"
};

int flag_fetch(enum flag_index index)
{
	if (index < 0 || index >= FLAG_MAX_FLAG) {
		printf("Flag index %d larger than max %d.\n",
			index, FLAG_MAX_FLAG);
		return -1;
	}

	if (index == FLAG_ECINRW) {
		if (CONFIG_EC_SOFTWARE_SYNC) {
			pch_gpio_direction(21, 1);
			return pch_gpio_get_value(21);
		} else {
			printf("EC software sync disabled.\n");
		}
	}

	const char const *name = cb_gpio_name[index];
	if (name == NULL) {
		printf("Don't have a gpio name for flag %d.\n", index);
		return -1;
	}

	struct cb_gpio *gpio = sysinfo_lookup_gpio(name);
	if (gpio == NULL)
		return -1;

	int p = (gpio->polarity == CB_GPIO_ACTIVE_HIGH) ? 0 : 1;
	return p ^ gpio->value;
}

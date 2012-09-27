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

#include "base/gpio.h"

int gpio_fetch(enum gpio_index index, gpio_t *gpio)
{
	const char const *name[GPIO_MAX_GPIO] = {
		"write protect",
		"recovery",
		"developer",
		"lid",
		"power",
	};

	int i;

	if (index < 0 || index >= GPIO_MAX_GPIO) {
		printf("index out of range: %d\n", index);
		return -1;
	}

	for (i = 0; i < lib_sysinfo.num_gpios; i++) {
		int p;

		if (strncmp((char *)lib_sysinfo.gpios[i].name, name[index],
						GPIO_MAX_NAME_LENGTH))
			continue;

		/* Entry found */
		gpio->index = index;
		gpio->port = lib_sysinfo.gpios[i].port;
		gpio->polarity = lib_sysinfo.gpios[i].polarity;
		gpio->value = lib_sysinfo.gpios[i].value;

		p = (gpio->polarity == GPIO_ACTIVE_HIGH) ? 0 : 1;
		gpio->value = p ^ gpio->value;

		return 0;
	}

	printf("failed to find gpio port\n");
	return -1;
}

int gpio_dump(gpio_t *gpio)
{
#ifdef VBOOT_DEBUG
	const char const *name[GPIO_MAX_GPIO] = {
		"wpsw", "recsw", "devsw", "lidsw", "pwrsw"
	};
	int index = gpio->index;

	if (index < 0 || index >= GPIO_MAX_GPIO) {
		printf(PREFIX "index out of range: %d\n", index);
		return -1;
	}

	printf(PREFIX "%-6s: port=%3d, polarity=%d, value=%d\n",
			name[gpio->index],
			gpio->port, gpio->polarity, gpio->value);
#endif
	return 0;
}

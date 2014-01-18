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

#include <assert.h>
#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/gpio/gpio.h"
#include "vboot/util/flag.h"

typedef struct SysinfoGpio {
	GpioOps ops;
	const char *label;
	struct cb_gpio *cb_gpio_ptr;
	int val;
} SysinfoGpio;

struct cb_gpio *sysinfo_lookup_gpio(const char *name)
{
	for (int i = 0; i < lib_sysinfo.num_gpios; i++) {
		if (!strncmp((char *)lib_sysinfo.gpios[i].name, name,
				CB_GPIO_MAX_NAME_LENGTH))
			return &lib_sysinfo.gpios[i];
	}

	printf("Failed to find gpio %s\n", name);
	return NULL;
}

static int sysinfo_gpio_get(struct GpioOps *me)
{
	assert(me);
	SysinfoGpio *sgpio = container_of(me, SysinfoGpio, ops);

	if (!sgpio->cb_gpio_ptr) {
		struct cb_gpio *cb_gpio_ptr =
			sysinfo_lookup_gpio(sgpio->label);
		if (!cb_gpio_ptr)
			return -1;

		sgpio->cb_gpio_ptr = cb_gpio_ptr;
		int p = (cb_gpio_ptr->polarity == CB_GPIO_ACTIVE_HIGH) ? 0 : 1;
		sgpio->val = p ^ cb_gpio_ptr->value;
	}

	return sgpio->val;
}

static SysinfoGpio write_protect = {
	.ops = {
		.get = &sysinfo_gpio_get
	},
	.label = "write protect"
};
GpioOps *sysinfo_write_protect = &write_protect.ops;

static SysinfoGpio recovery = {
	.ops = {
		.get = &sysinfo_gpio_get
	},
	.label = "recovery"
};
GpioOps *sysinfo_recovery = &recovery.ops;

static SysinfoGpio developer = {
	.ops = {
		.get = &sysinfo_gpio_get
	},
	.label = "developer"
};
GpioOps *sysinfo_developer = &developer.ops;

static SysinfoGpio lid = {
	.ops = {
		.get = &sysinfo_gpio_get
	},
	.label = "lid"
};
GpioOps *sysinfo_lid = &lid.ops;

static SysinfoGpio power = {
	.ops = {
		.get = &sysinfo_gpio_get
	},
	.label = "power"
};
GpioOps *sysinfo_power = &power.ops;

static SysinfoGpio oprom = {
	.ops = {
		.get = &sysinfo_gpio_get
	},
	.label = "oprom"
};
GpioOps *sysinfo_oprom = &oprom.ops;

void sysinfo_install_flags(void)
{
	flag_install(FLAG_WPSW, sysinfo_write_protect);
	flag_install(FLAG_RECSW, sysinfo_recovery);
	flag_install(FLAG_DEVSW, sysinfo_developer);
	flag_install(FLAG_LIDSW, sysinfo_lid);
	flag_install(FLAG_PWRSW, sysinfo_power);
	flag_install(FLAG_OPROM, sysinfo_oprom);
}

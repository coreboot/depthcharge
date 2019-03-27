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

#include <assert.h>
#include <libpayload.h>

#include "drivers/gpio/sysinfo.h"
#include "vboot/util/flag.h"

GpioOps *sysinfo_lookup_gpio(const char *name_to_find, int resample_at_runtime,
			     new_gpio_from_coreboot_t new_gpio_from_cb)
{
	struct cb_gpio *cb = lib_sysinfo.gpios;
	for (; cb - lib_sysinfo.gpios < lib_sysinfo.num_gpios; cb++) {
		if (strncmp((char *)cb->name, name_to_find,
			     CB_GPIO_MAX_NAME_LENGTH))
			continue;

		if (resample_at_runtime) {
			if ((int)cb->port == -1) {
				printf("WARNING: can't convert coreboot GPIOs, '%s' won't be resampled at runtime!\n",
				       cb->name);
			} else if (new_gpio_from_cb) {
				GpioOps *dc_gpio = new_gpio_from_cb(cb->port);
				if (cb->polarity == CB_GPIO_ACTIVE_LOW)
					dc_gpio = new_gpio_not(dc_gpio);
				return dc_gpio;
			}
		}

		uint32_t value = cb->value;
		die_if((int)value == -1, "coreboot did not sample '%s' GPIO!\n",
		       cb->name);
		if (cb->polarity == CB_GPIO_ACTIVE_LOW)
			value = !value;
		if (value)
			return new_gpio_high();
		else
			return new_gpio_low();
	}

	return NULL;
}

void sysinfo_install_flags(new_gpio_from_coreboot_t ngfc)
{
	/* If a GPIO is not defined, we will just flag_install() a NULL, which
	 * will only hit a die_if() if that flag is actually flag_fetch()ed. */
	flag_install(FLAG_WPSW, sysinfo_lookup_gpio("write protect", 0, ngfc));
	flag_install(FLAG_RECSW, sysinfo_lookup_gpio("recovery", 0, ngfc));
	flag_install(FLAG_OPROM, sysinfo_lookup_gpio("oprom", 0, ngfc));

	flag_install(FLAG_LIDSW, sysinfo_lookup_gpio("lid", 1, ngfc));
	flag_install(FLAG_PWRSW, sysinfo_lookup_gpio("power", 1, ngfc));
	flag_install(FLAG_ECINRW, sysinfo_lookup_gpio("EC in RW", 1, ngfc));
	flag_install(FLAG_PHYS_PRESENCE,
		     sysinfo_lookup_gpio("presence", 1, ngfc));
}

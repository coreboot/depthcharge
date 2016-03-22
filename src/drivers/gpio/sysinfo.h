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

#ifndef __DRIVERS_GPIO_SYSINFO_H__
#define __DRIVERS_GPIO_SYSINFO_H__

#include <libpayload.h>

#include "drivers/gpio/gpio.h"

typedef GpioOps *(*new_gpio_from_coreboot_t)(uint32_t port);

GpioOps *sysinfo_lookup_gpio(const char *name, int resample_at_runtime,
			      new_gpio_from_coreboot_t new_gpio_from_coreboot);
void sysinfo_install_flags(new_gpio_from_coreboot_t new_gpio_input_from_cb);

#endif /* __DRIVERS_GPIO_SYSINFO_H__ */

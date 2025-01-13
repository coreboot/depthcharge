/*
 * Copyright 2017 Google LLC
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

#ifndef __EC_PWM_BACKLIGHT_H
#define __EC_PWM_BACKLIGHT_H

#include "drivers/ec/cros/ec.h"
#include "drivers/gpio/gpio.h"

GpioOps *new_ec_pwm_backlight(void);

#endif

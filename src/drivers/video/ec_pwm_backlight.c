/*
 * Copyright 2017 Google Inc.
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

#include "drivers/video/ec_pwm_backlight.h"
#include "drivers/video/rk3399.h"

// Set backlight to 80% by default when on
// This mirrors default value from the kernel
// from internal_backlight_no_als_ac_brightness
#define DEFAULT_EC_BL_PWM_DUTY 80

static inline int switch_backlight_pwm(GpioOps *me, unsigned enable)
{
	return cros_ec_set_bl_pwm_duty(enable ? DEFAULT_EC_BL_PWM_DUTY : 0);
}

GpioOps *new_ec_pwm_backlight(void)
{
	GpioOps *ops = xzalloc(sizeof(*ops));
	ops->set = &switch_backlight_pwm;
	return ops;
}

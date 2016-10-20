/*
 * Copyright 2014 Rockchip Electronics Co., Ltd.
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

#include <arch/io.h>
#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/video/rk3399.h"

#define RK_CLRSETBITS(clr, set) ((((clr) | (set)) << 16) | set)
#define RK_SETBITS(set) RK_CLRSETBITS(0, set)
#define RK_CLRBITS(clr) RK_CLRSETBITS(clr, 0)

#define VOP_STANDBY_EN    22

// Set backlight to 80% by default when on
// This mirrors default value from the kernel
// from internal_backlight_no_als_ac_brightness
#define DEFAULT_EC_BL_PWM_DUTY 80

static uint32_t *vop0_sys_ctrl = (uint32_t *)0xff8f0008;
static uint32_t *vop1_sys_ctrl = (uint32_t *)0xff900008;

/*
 * callback to turn on/off the backlight
 */
static int rk3399_backlight_update(DisplayOps *me, uint8_t enable)
{
	return cros_ec_set_bl_pwm_duty(enable ? DEFAULT_EC_BL_PWM_DUTY : 0);
}

static int rockchip_display_stop(DisplayOps *me)
{
	/* set vop0 to standby */
	writel(RK_SETBITS(1 << VOP_STANDBY_EN), vop0_sys_ctrl);

	/* set vop1 to standby */
	writel(RK_SETBITS(1 << VOP_STANDBY_EN), vop1_sys_ctrl);

	/* wait frame complete (60Hz) to enter standby */
	mdelay(17);

	return 0;
}

DisplayOps *new_rk3399_display(void)
{
	DisplayOps *display = xzalloc(sizeof(*display));

	display->stop = rockchip_display_stop;
	display->backlight_update = rk3399_backlight_update;

	return display;
}

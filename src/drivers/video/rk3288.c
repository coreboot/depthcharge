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
#include "drivers/video/rk3288.h"

typedef struct {
	DisplayOps ops;
	GpioOps *backlight_gpio;
} RkDisplay;

#define RK_CLRSETBITS(clr, set) ((((clr) | (set)) << 16) | set)
#define RK_SETBITS(set) RK_CLRSETBITS(0, set)
#define RK_CLRBITS(clr) RK_CLRSETBITS(clr, 0)

#define VOP_STANDBY_EN    22

static uint32_t *vop0_sys_ctrl = (uint32_t *)0xff930008;
static uint32_t *vop1_sys_ctrl = (uint32_t *)0xff940008;

static int rk3288_backlight_update(DisplayOps *me, uint8_t enable)
{
	RkDisplay *display = container_of(me, RkDisplay, ops);
	gpio_set(display->backlight_gpio, enable);
	return 0;
}

static int rk3288_display_stop(DisplayOps *me)
{
	/* set vop0 to standby */
	writel(RK_SETBITS(1 << VOP_STANDBY_EN), vop0_sys_ctrl);

	/* set vop1 to standby */
	writel(RK_SETBITS(1 << VOP_STANDBY_EN), vop1_sys_ctrl);

	/* wait frame complete (60Hz) to enter standby */
	mdelay(17);

	return 0;
}

DisplayOps *new_rk3288_display(GpioOps *backlight)
{
	RkDisplay *display = xzalloc(sizeof(*display));
	display->ops.stop = rk3288_display_stop;
	if (backlight) {
		display->backlight_gpio = backlight;
		display->ops.backlight_update = rk3288_backlight_update;
	}

	return &display->ops;
}

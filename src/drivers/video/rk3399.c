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

#define VOP_STANDBY_EN		1
#define VOP_STANDBY_OFFSET	22

// Set backlight to 80% by default when on
// This mirrors default value from the kernel
// from internal_backlight_no_als_ac_brightness
#define DEFAULT_EC_BL_PWM_DUTY 80

/* video_ctl_1 */
#define VIDEO_EN				(0x1 << 7)

/* sys_ctl_3 */
#define STRM_VALID                              (0x1 << 2)

static uint32_t *edp_video_ctl_1 = (uint32_t *)0xff970020;
static uint32_t *edp_sys_ctl_3 = (uint32_t *)0xff970608;
static uint32_t *vop0_sys_ctrl = (uint32_t *)0xff900008;
static uint32_t *vop0_win0_yrgb_mst = (uint32_t *)0xff900040;
static uint32_t *vop0_reg_cfg_done = (uint32_t *)0xff900000;

static int edp_is_video_stream_on(void)
{
	u32 val;
	u64 start = timer_us(0);

	do {
		val = readl(edp_sys_ctl_3);

		/* must write value to update STRM_VALID bit status */
		writel(val, edp_sys_ctl_3);
		val = readl(edp_sys_ctl_3);
		if (!(val & STRM_VALID))
			return 0;
	} while (timer_us(start) < 100 * 1000);

	return -1;
}

static int edp_enable(void)
{
	/* Enable video at next frame */
	setbits_le32(edp_video_ctl_1, VIDEO_EN);

	return edp_is_video_stream_on();
}

/*
 * callback to turn on/off the backlight
 */
static int rk3399_backlight_update(DisplayOps *me, uint8_t enable)
{
	return cros_ec_set_bl_pwm_duty(enable ? DEFAULT_EC_BL_PWM_DUTY : 0);
}

static int rockchip_display_init(DisplayOps *me)
{
	uintptr_t phys_addr = lib_sysinfo.framebuffer->physical_address;

	if (edp_enable())
		return -1;

	writel(phys_addr, vop0_win0_yrgb_mst);

	/* enable reg config */
	writel(0xffff, vop0_reg_cfg_done);

	return 0;
}

static int rockchip_display_stop(DisplayOps *me)
{
	/* set vop0 to standby */
	setbits_le32(vop0_sys_ctrl, VOP_STANDBY_EN << VOP_STANDBY_OFFSET);

	/* wait frame complete (60Hz) to enter standby */
	mdelay(17);

	return 0;
}

DisplayOps *new_rk3399_display(void)
{
	DisplayOps *display = xzalloc(sizeof(*display));

	display->init = rockchip_display_init;
	display->stop = rockchip_display_stop;
	display->backlight_update = rk3399_backlight_update;

	return display;
}

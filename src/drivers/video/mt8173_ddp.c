/*
 * Copyright 2015 Google Inc.
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
#include <libpayload.h>
#include "mt8173_ddp.h"

enum {
	DISPSYS_OVL_BASE     = 0x1400C000,
	DISP_REG_OVL_STA     = (DISPSYS_OVL_BASE + 0x0000),
	DISP_REG_OVL_EN      = (DISPSYS_OVL_BASE + 0x000C),
	DISP_REG_OVL_L0_ADDR = (DISPSYS_OVL_BASE + 0x0f40)
};


static int mt8173_display_init(DisplayOps *me)
{
	uintptr_t phys_addr = lib_sysinfo.framebuffer->physical_address;

	writel(phys_addr, (void *)(uintptr_t)DISP_REG_OVL_L0_ADDR);
	writel(0x01, (void *)(uintptr_t)DISP_REG_OVL_EN);

	return 0;
}

static int mt8173_display_stop(DisplayOps *me)
{
	/* Disable overlayer 0 */
	writel(0x00, (void *)(uintptr_t)DISP_REG_OVL_EN);

	return 0;
}

DisplayOps *new_mt8173_display(int (*backlight_update)
			       (DisplayOps *me, uint8_t enable))
{
	DisplayOps *display_ops = xzalloc(sizeof(DisplayOps));
	display_ops->init = mt8173_display_init;
	display_ops->stop = mt8173_display_stop;
	if (backlight_update)
		display_ops->backlight_update = backlight_update;

	return display_ops;
}

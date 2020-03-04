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
#include <base/container_of.h>
#include "mtk_ddp.h"

typedef struct {
	DisplayOps ops;
	uintptr_t ovl_base;
	int lanes;
} MtkDisplay;

enum {
	DISP_REG_OVL_STA     = 0x0000,
	DISP_REG_OVL_EN      = 0x000C,
	DISP_REG_OVL_L0_ADDR = 0x0f40,
	DISP_REG_OVL0_2L_EN  = 0x100C,
};

static int mtk_display_init(DisplayOps *me)
{
	uintptr_t phys_addr = lib_sysinfo.framebuffer->physical_address;
	MtkDisplay *mtk = container_of(me, MtkDisplay, ops);

	writel(phys_addr, (void *)(mtk->ovl_base + DISP_REG_OVL_L0_ADDR));
	writel(1, (void *)(mtk->ovl_base + DISP_REG_OVL_EN));
	if (mtk->lanes == 2)
		writel(1, (void *)(mtk->ovl_base + DISP_REG_OVL0_2L_EN));

	return 0;
}

static int mtk_display_stop(DisplayOps *me)
{
	MtkDisplay *mtk = container_of(me, MtkDisplay, ops);

	writel(0, (void *)(mtk->ovl_base + DISP_REG_OVL_EN));
	if (mtk->lanes == 2)
		writel(0, (void *)(mtk->ovl_base + DISP_REG_OVL0_2L_EN));

	return 0;
}

DisplayOps *new_mtk_display(int (*backlight_update_fn)
			    (DisplayOps *me, uint8_t enable),
			    uintptr_t ovl_base, int lanes)
{
	MtkDisplay *display = xzalloc(sizeof(MtkDisplay));

	display->ops.init = mtk_display_init;
	display->ops.stop = mtk_display_stop;
	display->ovl_base = ovl_base;
	display->lanes = lanes;
	if (backlight_update_fn)
		display->ops.backlight_update = backlight_update_fn;

	return &display->ops;
}

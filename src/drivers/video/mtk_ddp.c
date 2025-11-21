/*
 * Copyright 2015 Google LLC
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
#include "mtk_ddp.h"

enum ovl_type {
	DISP_OVL = 0,
	DISP_EXDMA,
};

typedef struct {
	DisplayOps ops;
	uintptr_t ovl_base;
	uintptr_t blender_base;
	uintptr_t ovl_base1;
	uintptr_t blender_base1;
	int lanes;
	enum ovl_type type;
} MtkDisplay;

enum {
	DISP_REG_OVL_STA     = 0x0000,
	DISP_REG_OVL_EN      = 0x000C,
	DISP_REG_OVL_L0_ADDR = 0x0f40,
	DISP_REG_OVL0_2L_EN  = 0x100C,
	DISP_REG_EXDMA_EN    = 0x0020,
	DISP_REG_EXDMA_L_EN  = 0x0040,
};

static int mtk_display_init(DisplayOps *me)
{
	uintptr_t phys_addr = lib_sysinfo.framebuffer.physical_address;
	MtkDisplay *mtk = container_of(me, MtkDisplay, ops);
	uint32_t offset;

	if (mtk->type == DISP_OVL) {
		write32p(mtk->ovl_base + DISP_REG_OVL_L0_ADDR, phys_addr);
		write32p(mtk->ovl_base + DISP_REG_OVL_EN, 1);
		if (mtk->lanes == 2)
			write32p(mtk->ovl_base + DISP_REG_OVL0_2L_EN, 1);
	} else {
		write32p(mtk->ovl_base + DISP_REG_OVL_L0_ADDR, phys_addr);
		setbits32p(mtk->ovl_base + DISP_REG_EXDMA_EN, BIT(0));
		setbits32p(mtk->ovl_base + DISP_REG_EXDMA_L_EN, BIT(0));
		setbits32p(mtk->blender_base + DISP_REG_EXDMA_L_EN, BIT(0));

		if (!mtk->ovl_base1)
			return 0;

		offset = lib_sysinfo.framebuffer.x_resolution *
			 lib_sysinfo.framebuffer.bits_per_pixel / 8 / 2;
		write32p(mtk->ovl_base1 + DISP_REG_OVL_L0_ADDR, phys_addr + offset);
		setbits32p(mtk->ovl_base1 + DISP_REG_EXDMA_EN, BIT(0));
		setbits32p(mtk->ovl_base1 + DISP_REG_EXDMA_L_EN, BIT(0));
		setbits32p(mtk->blender_base1 + DISP_REG_EXDMA_L_EN, BIT(0));
	}

	return 0;
}

static int mtk_display_stop(DisplayOps *me)
{
	MtkDisplay *mtk = container_of(me, MtkDisplay, ops);

	if (mtk->type == DISP_OVL) {
		write32p(mtk->ovl_base + DISP_REG_OVL_EN, 0);
		if (mtk->lanes == 2)
			write32p(mtk->ovl_base + DISP_REG_OVL0_2L_EN, 0);
	} else {
		clrbits32p(mtk->blender_base + DISP_REG_EXDMA_L_EN, BIT(0));
		clrbits32p(mtk->ovl_base + DISP_REG_EXDMA_EN, BIT(0));
		clrbits32p(mtk->ovl_base + DISP_REG_EXDMA_L_EN, BIT(0));

		if (!mtk->ovl_base1)
			return 0;

		clrbits32p(mtk->blender_base1 + DISP_REG_EXDMA_L_EN, BIT(0));
		clrbits32p(mtk->ovl_base1 + DISP_REG_EXDMA_EN, BIT(0));
		clrbits32p(mtk->ovl_base1 + DISP_REG_EXDMA_L_EN, BIT(0));
	}

	return 0;
}

DisplayOps *new_mtk_display(int (*backlight_update_fn)
			    (DisplayOps *me, bool enable),
			    uintptr_t ovl_base, int lanes,
			    uintptr_t blender_base,
			    uintptr_t ovl_base1,
			    uintptr_t blender_base1)
{
	MtkDisplay *display = xzalloc(sizeof(MtkDisplay));

	display->ops.init = mtk_display_init;
	display->ops.stop = mtk_display_stop;

	display->type = blender_base ? DISP_EXDMA : DISP_OVL;
	display->blender_base = blender_base;
	display->ovl_base = ovl_base;
	display->blender_base1 = blender_base1;
	display->ovl_base1 = ovl_base1;
	display->lanes = lanes;
	if (backlight_update_fn)
		display->ops.backlight_update = backlight_update_fn;

	return &display->ops;
}

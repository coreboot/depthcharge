/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2022, The Linux Foundation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <arch/io.h>
#include <libpayload.h>
#include "drivers/video/sc7280.h"
#include "drivers/gpio/gpio.h"

typedef struct {
	DisplayOps ops;
	GpioOps *backlight_gpio;
} SC7280Display;

struct uint32_t *timing_engine_en = (void *)0x0AE3A000;
struct uint32_t *sspp_src_addr = (void *)0x0AE05014;
struct uint32_t *edp_sw_rest = (void *)0xAEA0010;

static int sc7280_backlight_update(DisplayOps *me, bool enable)
{
	SC7280Display *display = container_of(me, SC7280Display, ops);

	if (display->backlight_gpio)
		gpio_set(display->backlight_gpio, enable);

	return 0;
}

static int sc7280_display_stop(DisplayOps *me)
{
	/* Turnoff Backlight */
	printf("%s:display stop.\n", __func__);
	sc7280_backlight_update(me, 0);

	/* Disable TE */
	write32(timing_engine_en, 0x0);

	/*wait for a vsync pulse */
	mdelay(20);

	/* Reset eDP */
	write32(edp_sw_rest, 0x1);
	mdelay(20);
	write32(edp_sw_rest, 0x0);

	return 0;
}

static int sc7280_display_init(DisplayOps *me)
{
	uintptr_t phys_addr = lib_sysinfo.framebuffer.physical_address;

	printf("%s: program fb & start display.\n", __func__);

	/* Program framebuffer address */
	write32(sspp_src_addr, phys_addr);

	/*Start Timing Engine*/
	write32(timing_engine_en, 0x01);

	return 0;
}

DisplayOps *new_sc7280_display(GpioOps *backlight)
{
	SC7280Display *display = xzalloc(sizeof(*display));

	display->ops.init = sc7280_display_init;
	display->ops.stop = sc7280_display_stop;

	if (backlight) {
		display->backlight_gpio = backlight;
		display->ops.backlight_update = sc7280_backlight_update;
	}

	return &display->ops;
}

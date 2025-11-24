/*
 * Copyright 2014 Google LLC
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
#include <stdint.h>

#include "base/cleanup_funcs.h"
#include "drivers/video/display.h"
#include "vboot/ui.h"

static DisplayOps *display_ops;

static int display_cleanup(struct CleanupFunc *cleanup, CleanupType type)
{
	int err = ui_display_clear();
	if (type != CleanupOnLegacy)
		err |= display_stop();
	return err ? -1 : 0;
}

static CleanupFunc display_cleanup_func = {
	.cleanup = &display_cleanup,
	.types = CleanupOnReboot | CleanupOnPowerOff |
		 CleanupOnHandoff | CleanupOnLegacy,
};

void display_set_ops(DisplayOps *ops)
{
	die_if(display_ops, "%s: Display ops already set.\n", __func__);
	display_ops = ops;
}

int display_init(void)
{
	/*
	 * If the framebuffer is unavailable (e.g., no display hardware
	 * detected in the device tree), skip probing the display. This
	 * avoids calls to various display related functions and
	 * preventing redundant errors in boot log.
	 */
	if (!display_init_required()) {
		printf("display is not available hence, skip calling other display functions.\n");
		return -1;
	}

	if (display_ops && display_ops->init) {
		if (display_ops->init(display_ops))
			return -1;
	} else {
		printf("display: %s called but not implemented.\n", __func__);
	}

	/* Call stop() when exiting depthcharge, only if we initialized it. */
	list_insert_after(&display_cleanup_func.list_node, &cleanup_funcs);
	return 0;
}

int display_init_required(void)
{
	return lib_sysinfo.framebuffer.physical_address != 0;
}

int has_external_display(void)
{
	return lib_sysinfo.framebuffer.flags.has_external_display;
}

int backlight_update(bool enable)
{
	if (display_ops && display_ops->backlight_update)
		return display_ops->backlight_update(display_ops, enable);

	printf("display: %s called but not implemented.\n", __func__);
	return 0;
}

int display_screen(enum ui_screen screen)
{
	if (display_ops && display_ops->display_screen)
		return display_ops->display_screen(display_ops, screen);

	printf("display: %s called but not implemented.\n", __func__);
	return 0;
}

int display_stop(void)
{
	int err;

	err = backlight_update(false);
	if (display_ops && display_ops->stop)
		err |= display_ops->stop(display_ops);

	return err;
}

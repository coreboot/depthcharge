/*
 * Copyright 2014 Google Inc.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>
#include <stdint.h>

#include "base/cleanup_funcs.h"
#include "drivers/video/display.h"

static DisplayOps *display_ops;

static int display_cleanup(struct CleanupFunc *cleanup, CleanupType type)
{
	if (display_ops != NULL && display_ops->stop != NULL)
		return display_ops->stop(display_ops);
	return 0;
}

static CleanupFunc display_cleanup_func = {
	.cleanup = &display_cleanup,
	.types = CleanupOnHandoff,
};

void display_set_ops(DisplayOps *ops)
{
	die_if(display_ops, "%s: Display ops already set.\n", __func__);
	display_ops = ops;

	/* Call stop() when exiting depthcharge. */
	list_insert_after(&display_cleanup_func.list_node, &cleanup_funcs);
}

int display_init(void)
{
	if (display_ops != NULL && display_ops->init != NULL)
		return display_ops->init(display_ops);

	return 0;
}

int backlight_update(uint8_t enable)
{
	if (display_ops != NULL && display_ops->backlight_update != NULL)
		return display_ops->backlight_update(display_ops, enable);

	printf("%s called but not implemented.\n", __func__);
	return 0;
}


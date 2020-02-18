// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google Inc.
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

#include <vb2_api.h>

#include "drivers/video/display.h"
#include "vboot/ui.h"

static vb2_error_t vboot_draw_blank(const struct ui_state *state)
{
	clear_screen(&ui_color_bg);
	return VB2_SUCCESS;
}

/*
 * TODO(chromium:1035800): Refactor UI code across vboot and depthcharge.
 * Currently vboot and depthcharge maintain their own copies of menus/screens.
 * vboot detects keyboard input and controls the navigation among different menu
 * items and screens, while depthcharge performs the actual rendering of each
 * screen, based on the menu information passed from vboot.
 */
static const struct ui_descriptor vboot_screens[] = {
	{
		.screen = VB2_SCREEN_BLANK,
		.draw = vboot_draw_blank,
		.mesg = NULL,
	},
};

const struct ui_descriptor *ui_get_descriptor(enum vb2_screen screen)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(vboot_screens); i++) {
		if (vboot_screens[i].screen == screen)
			return &vboot_screens[i];
	}
	return NULL;
}

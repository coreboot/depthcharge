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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>
#include <vboot_api.h>
#include <vboot/screens.h>
#include "drivers/video/display.h"

static char initialized = 0;

static VbError_t draw_screen(uint32_t screen_type, uint32_t locale)
{
	VbError_t rv = VBERROR_SUCCESS;

	switch (screen_type) {
	case VB_SCREEN_BLANK:
	case VB_SCREEN_DEVELOPER_WARNING:
	case VB_SCREEN_RECOVERY_REMOVE:
	case VB_SCREEN_RECOVERY_NO_GOOD:
	case VB_SCREEN_RECOVERY_INSERT:
	case VB_SCREEN_RECOVERY_TO_DEV:
	case VB_SCREEN_DEVELOPER_TO_NORM:
	case VB_SCREEN_WAIT:
	case VB_SCREEN_TO_NORM_CONFIRMED:
	default:
		printf("Not a valid screen type: 0x%x\n", screen_type);
		return VBERROR_INVALID_SCREEN_INDEX;
	}

	return rv;
}

static VbError_t vboot_init_display(void)
{
	if (display_init())
		return VBERROR_UNKNOWN;

	/* initialize video console */
	video_init();
	video_console_clear();
	video_console_cursor_enable(0);
	initialized = 1;

	return VBERROR_SUCCESS;
}

int vboot_draw_screen(uint32_t screen)
{
	static uint32_t current_screen = VB_SCREEN_BLANK;
	static uint32_t current_locale = 0;
	/* TODO: Read locale from nvram */
	uint32_t locale = 0;
	VbError_t rv;

	printf("%s: screen=0x%x locale=%d\n", __func__, screen, locale);

	if (!initialized) {
		if (vboot_init_display())
			return VBERROR_UNKNOWN;
	}

	/* If requested screen is the same as the current one, we're done. */
	if (screen == current_screen && locale == current_locale)
		return VBERROR_SUCCESS;

	/* If the screen is blank, turn off the backlight; else turn it on. */
	backlight_update(VB_SCREEN_BLANK == screen ? 0 : 1);

	/* TODO: draw only locale dependent part if current_screen == screen */
	rv = draw_screen(screen, locale);
	if (rv)
		return rv;

	current_screen = screen;
	current_locale = locale;

	return VBERROR_SUCCESS;
}

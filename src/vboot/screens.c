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

static VbError_t vboot_draw_blank(uint32_t locale)
{
	video_console_clear();
	return VBERROR_SUCCESS;
}

/* we may export this in the future for the board customization */
struct vboot_screen_descriptor {
	uint32_t id;				/* VB_SCREEN_* */
	VbError_t (*draw)(uint32_t locale);	/* draw function */
	const char *mesg;			/* fallback message */
};

static const struct vboot_screen_descriptor vboot_screens[] = {
	{
		.id = VB_SCREEN_BLANK,
		.draw = vboot_draw_blank,
		.mesg = NULL,
	},
	{
		.id = VB_SCREEN_DEVELOPER_WARNING,
		.draw = NULL,
		.mesg = "OS verification is OFF\n"
			"Press SPACE to re-enable.\n",
	},
	{
		.id = VB_SCREEN_RECOVERY_REMOVE,
		.draw = NULL,
		.mesg = "Please remove all external devices to begin recovery\n",
	},
	{
		.id = VB_SCREEN_RECOVERY_NO_GOOD,
		.draw = NULL,
		.mesg = "The device you inserted does not contain Chrome OS.\n",
	},
	{
		.id = VB_SCREEN_RECOVERY_INSERT,
		.draw = NULL,
		.mesg = "Chrome OS is missing or damaged.\n"
			"Please insert a recovery USB stick or SD card.\n",
	},
	{
		.id = VB_SCREEN_RECOVERY_TO_DEV,
		.draw = NULL,
		.mesg = "To turn OS verificaion OFF, press ENTER.\n"
			"Your system will reboot and local data will be cleared.\n"
			"To go back, press ESC.\n",
	},
	{
		.id = VB_SCREEN_DEVELOPER_TO_NORM,
		.draw = NULL,
		.mesg = "OS verification is OFF\n"
			"Press ENTER to confirm you wish to turn OS verification on.\n"
			"Your system will reboot and local data will be cleared.\n"
			"To go back, press ESC.\n",
	},
	{
		.id = VB_SCREEN_WAIT,
		.draw = NULL,
		.mesg = "Your system is applying a critical update.\n"
			"Please do not turn off.\n",
	},
	{
		.id = VB_SCREEN_TO_NORM_CONFIRMED,
		.draw = NULL,
		.mesg = "OS verification is ON\n"
			"Your system will reboot and local data will be cleared.\n",
	},
};

static const struct vboot_screen_descriptor *get_screen_descriptor(uint32_t id)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(vboot_screens); i++) {
		if (vboot_screens[i].id == id)
			return &vboot_screens[i];
	}
	return NULL;
}

static void print_fallback_message(const struct vboot_screen_descriptor *desc)
{
	const struct rgb_color white = { 0xff, 0xff, 0xff };
	if (clear_screen(&white))
		return;
	if (desc->mesg) {
		unsigned int rows, cols;
		video_get_rows_cols(&rows, &cols);
		video_console_set_cursor(0, rows/2);
		video_printf(0, 15, VIDEO_PRINTF_ALIGN_CENTER, desc->mesg);
	}
}

static VbError_t draw_screen(uint32_t screen_type, uint32_t locale)
{
	VbError_t rv = VBERROR_UNKNOWN;
	const struct vboot_screen_descriptor *desc;

	desc = get_screen_descriptor(screen_type);
	if (!desc) {
		printf("Not a valid screen type: 0x%x\n", screen_type);
		return VBERROR_INVALID_SCREEN_INDEX;
	}

	/* if no drawing function is registered, fallback msg will be printed */
	if (desc->draw)
		rv = desc->draw(locale);
	if (rv)
		print_fallback_message(desc);

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

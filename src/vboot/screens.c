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

#include <cbfs.h>
#include <libpayload.h>
#include <vboot_api.h>

#include "drivers/video/display.h"

#define FB_INFO_POSITION_ROW 30
#define FB_INFO_FOREGROUND 11
#define FB_INFO_BACKGROUND 0

static uint32_t current_screen = VB_SCREEN_BLANK;
static char initialized = 0;

static inline void *load_bitmap(const char *name, uint32_t *size)
{
	struct cbfs_file *file = cbfs_get_file(CBFS_DEFAULT_MEDIA, name);
	if (file == NULL) {
		printf("Could not find file '%s'.\n", name);
		return NULL;
	}
	*size = ntohl(file->len);
	return cbfs_get_file_content(CBFS_DEFAULT_MEDIA, name, CBFS_TYPE_RAW);
}

static VbError_t fastboot_draw_base_screen(uint32_t localize)
{
	if (clear_screen(0x0, 0x0, 0x0))
		return VBERROR_UNKNOWN;

	/* 'the lightbar' (Red Yellow Blue Green) */
	draw_box(14, 16, 18, 1, 219, 65, 55);
	draw_box(32, 16, 18, 1, 244, 180, 0);
	draw_box(50, 16, 18, 1, 66, 133, 244);
	draw_box(68, 16, 18, 1, 15, 157, 88);

	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_base_screen(uint32_t localize)
{
	return fastboot_draw_base_screen(localize);
}

static VbError_t vboot_draw_developer_warning(uint32_t localize)
{
	VbError_t rv = vboot_draw_base_screen(localize);
	if (rv)
		return rv;

	video_console_set_cursor(0, FB_INFO_POSITION_ROW);
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 1,
		     "OS verification is OFF\n");
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 1,
		     "Press Volume Down to re-enable.\n");

	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_recovery_remove(uint32_t localize)
{
	VbError_t rv = vboot_draw_base_screen(localize);
	if (rv)
		return rv;

	video_console_set_cursor(0, FB_INFO_POSITION_ROW);
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 1,
		     "Remove inserted media\n");
	return rv;
}

static VbError_t vboot_draw_recovery_no_good(uint32_t localize)
{
	VbError_t rv = vboot_draw_base_screen(localize);
	if (rv)
		return rv;

	video_console_set_cursor(0, FB_INFO_POSITION_ROW);
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 1,
		     "The device you inserted does not contain Chrome OS.\n");
	return rv;
}

static VbError_t vboot_draw_recovery_insert(uint32_t localize)
{
	VbError_t rv = vboot_draw_base_screen(localize);
	if (rv)
		return rv;

	video_console_set_cursor(0, FB_INFO_POSITION_ROW);
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 1,
		     "Chrome OS is missing or damaged.\n");
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 1,
		     "Please insert a recovery USB stick.\n");
	return rv;
}

static VbError_t vboot_draw_recovery_to_dev(uint32_t localize)
{
	VbError_t rv = vboot_draw_base_screen(localize);
	if (rv)
		return rv;

	video_console_set_cursor(0, FB_INFO_POSITION_ROW);
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 1,
		     "To turn OS verificaion OFF, press Volume Up.\n");
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 1,
		     "Your system will reboot and local data will be cleared.\n");
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 1,
		     "To go back, press ESC.\n");
	return rv;
}

static VbError_t vboot_draw_developer_to_norm(uint32_t localize)
{
	VbError_t rv = vboot_draw_base_screen(localize);
	if (rv)
		return rv;

	video_console_set_cursor(0, FB_INFO_POSITION_ROW);
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 1,
		     "OS verification is OFF\n");
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 1,
		     "Press Volume Up to confirm you wish to turn OS verification on.\n");
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 1,
		     "Your system will reboot and local data will be cleared.\n");
	return rv;
}

static VbError_t vboot_draw_to_norm_confirmed(uint32_t localize)
{
	VbError_t rv = vboot_draw_base_screen(localize);
	if (rv)
		return rv;

	video_console_set_cursor(0, FB_INFO_POSITION_ROW);
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 1,
		     "OS verification is ON\n");
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 1,
		     "Your system will reboot and local data will be cleared.\n");
	return rv;
}

static VbError_t vboot_draw_fastboot_menu(uint32_t localize)
{
	VbError_t rv;
	uint32_t size;
	uint8_t *buf;

	rv = fastboot_draw_base_screen(localize);
	if (rv)
		return rv;

	buf = load_bitmap("arrow_up.bmp", &size);
	if (buf) {
		draw_bitmap(61, 20, 2, buf, size);
		free(buf);
	}
	video_console_set_cursor(102, 10);
	video_printf(15, 0, 0, "Volume Up: Run Selected Option");

	buf = load_bitmap("arrow_down.bmp", &size);
	if (buf) {
		draw_bitmap(61, 22, 2, buf, size);
		free(buf);
	}
	video_console_set_cursor(102, 11);
	video_printf(15, 0, 0, "Volume Down: Next Option");

	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_fastboot_mode(uint32_t localize)
{
	fastboot_draw_base_screen(localize);

	return VBERROR_SUCCESS;
}

static VbError_t draw_screen(uint32_t screen_type, uint32_t localize)
{
	VbError_t rv = VBERROR_SUCCESS;

	switch (screen_type) {
	case VB_SCREEN_BLANK:
		video_console_clear();
		break;
	case VB_SCREEN_FASTBOOT_MENU:
		rv = vboot_draw_fastboot_menu(localize);
		break;
	case VB_SCREEN_FASTBOOT_MODE:
		rv = vboot_draw_fastboot_mode(localize);
		break;
	case VB_SCREEN_DEVELOPER_WARNING:
		rv = vboot_draw_developer_warning(localize);
		break;
	case VB_SCREEN_RECOVERY_REMOVE:
		rv = vboot_draw_recovery_remove(localize);
		break;
	case VB_SCREEN_RECOVERY_NO_GOOD:
		rv = vboot_draw_recovery_no_good(localize);
		break;
	case VB_SCREEN_RECOVERY_INSERT:
		rv = vboot_draw_recovery_insert(localize);
		break;
	case VB_SCREEN_RECOVERY_TO_DEV:
		rv = vboot_draw_recovery_to_dev(localize);
		break;
	case VB_SCREEN_DEVELOPER_TO_NORM:
		rv = vboot_draw_developer_to_norm(localize);
		break;
	case VB_SCREEN_WAIT:
		break;
	case VB_SCREEN_TO_NORM_CONFIRMED:
		rv = vboot_draw_to_norm_confirmed(localize);
		break;
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

int vboot_draw_screen(uint32_t screen, uint32_t localize, int force)
{
	VbError_t rv;

	printf("%s: screen=0x%x force=%d\n", __func__, screen, force);

	if (!initialized) {
		if (vboot_init_display())
			return VBERROR_UNKNOWN;
	}

	/* If requested screen is the same as the current one, we're done. */
	if (current_screen == screen && !force)
		return VBERROR_SUCCESS;

	/* If the screen is blank, turn off the backlight; else turn it on. */
	backlight_update(VB_SCREEN_BLANK == screen ? 0 : 1);

	rv = draw_screen(screen, localize);
	if (!rv)
		current_screen = screen;

	return rv;
}

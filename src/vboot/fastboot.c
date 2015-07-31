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

#include <assert.h>
#include <libpayload.h>
#include <keycodes.h>
#include <vboot_nvstorage.h>
#include <vboot_struct.h>

#include "drivers/ec/cros/commands.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/power/power.h"
#include "drivers/video/coreboot_fb.h"
#include "drivers/video/display.h"
#include "fastboot/fastboot.h"
#include "vboot/stages.h"
#include "vboot/util/commonparams.h"

fb_ret_type __attribute__((weak)) device_mode_enter(void)
{
	return FB_CONTINUE_RECOVERY;
}

static void vboot_clear_recovery(void)
{
	vboot_update_recovery(VBNV_RECOVERY_NOT_REQUESTED);
}

static void vboot_set_recovery(void)
{
	vboot_update_recovery(VBNV_RECOVERY_FW_FASTBOOT);
}

enum {
	ENTRY_POINT_RECOVERY_MENU,
	ENTRY_POINT_FASTBOOT_MODE,
};

/*
 * Currently, all boards use EC to identify keyboard events. If in the future,
 * any other mechanism is used, every mechanism needs to have its own
 * implementation of this routine.
 */
uint8_t fastboot_keyboard_mask(void)
{
	uint32_t ec_events;
	const uint32_t kb_fastboot_mask =
		EC_HOST_EVENT_MASK(EC_HOST_EVENT_KEYBOARD_FASTBOOT);

	cros_ec_get_host_events(&ec_events);

	if (kb_fastboot_mask & ec_events) {
		cros_ec_clear_host_events(kb_fastboot_mask);
		return 1;
	}

	return 0;
}

static int is_fastboot_mode_requested(void)
{
	VbSharedDataHeader *vdat;
	int size;

	assert(find_common_params((void **)&vdat, &size) == 0);

	/*
	 * Enter fastboot mode when:
	 * 1. Recovery reason is set to fastboot requested by FW, or
	 * 2. Recovery reason is set to recovery requested by BCB, or
	 * 3. Recovery reason is set to manual request and keyboard mask is set
	 * to fastboot event.
	 *
	 * Else, enter recovery-menu to allow user to choose from different
	 * options.
	 */
	return ((vdat->recovery_reason == VBNV_RECOVERY_FW_FASTBOOT) ||
	    (vdat->recovery_reason == VBNV_RECOVERY_BCB_USER_MODE) ||
	    ((vdat->recovery_reason == VBNV_RECOVERY_RO_MANUAL) &&
	     fastboot_keyboard_mask()));
}

static void udc_fastboot(void)
{
	if (!board_should_enter_device_mode())
		return;

	vboot_clear_recovery();

	fb_ret_type ret = device_mode_enter();

	switch(ret) {
	case FB_REBOOT:
		/* Does not return */
		cold_reboot();
		break;
	case FB_REBOOT_BOOTLOADER:
		vboot_set_recovery();
		/* Does not return */
		cold_reboot();
		break;
	case FB_POWEROFF:
		/* Does not return */
		power_off();
		break;
	case FB_CONTINUE_RECOVERY:
		/* Continue in cros recovery mode */
		break;
	default:
		/* unknown error. reboot into fastboot menu */
		vboot_set_recovery();
		cold_reboot();
		break;
	}
}

static void menu_start(void)
{
	vboot_clear_recovery();
	cold_reboot();
}


static void menu_fastboot(void)
{
	udc_fastboot();
}

static void menu_reset(void)
{
	cold_reboot();
}

static void menu_power_off(void)
{
	power_off();
}

void vboot_try_fastboot(void)
{
	static const struct {
		const char *text;
		const int color; /* VGA color */
		void (*func)(void);
	} commands[] = {
		{ "Start", 10, menu_start },		/* green */
		{ "Enable fastboot", 7, menu_fastboot },
		{ "Enter CrOS recovery mode", 7, NULL},
		{ "Reset device", 12, menu_reset},	/* red */
		{ "Power off device", 12, menu_power_off},
	};

	if (is_fastboot_mode_requested()) {
		/* fastboot mode is explicitly requested. skip menu. */
		printf("entering fastboot mode\n");
		udc_fastboot();
		return;
	}

	const int command_count = ARRAY_SIZE(commands);
	int position = 0;

	int i;
	int max_strlen = 0;
	for (i = 0; i < command_count; i++) {
		int j = strlen(commands[i].text);
		if (j > max_strlen)
			max_strlen = j;
	}

	unsigned int rows, cols;
	if (display_init())
		return;

	if (backlight_update(1))
		return;

	video_init();
	video_console_clear();
	video_console_cursor_enable(0);

	video_get_rows_cols(&rows, &cols);

	video_console_set_cursor(0, 3);
	video_printf(7, 0, "<-- Button up: run selected option\n\n");
	video_printf(7, 0, "<-- Button down: next option\n");

	video_console_set_cursor(0, rows - 10);

	// set cursor appropriately
	video_printf(12, 0, "FASTBOOT MODE\n");
	/*
	 * TODO: Show PRODUCT_NAME, VARIANT, HW VERSION, BOOTLOADER VERSION,
	 * SERIAL NUMBER, SIGNING
	 */
	video_printf(12, 0, "DEVICE: %s\n",
		     fb_device_unlocked() ? "unlocked" : "locked");

	while (1) {
		video_console_set_cursor(0, 0);
		video_printf(commands[position].color, 0, "%*s",
			-max_strlen, commands[position].text);
		int keypress = getchar();
		printf("got keypress %x\n", keypress);
		if (keypress == ' ') {
			printf("moving forward\n");
			if (++position == command_count)
				position = 0;
		} else if (keypress & KEY_SELECT) {
			printf("selected %s\n", commands[position].text);
			if (commands[position].func == NULL)
				/* leave menu and continue to boot */
				break;
			commands[position].func();
			break;
		}
	}
	printf("leaving fastboot menu\n");
}

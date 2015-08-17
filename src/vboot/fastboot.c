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
#include <cbfs.h>

#include "config.h"
#include "drivers/ec/cros/commands.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/power/power.h"
#include "drivers/video/display.h"
#include "fastboot/fastboot.h"
#include "vboot/stages.h"
#include "vboot/firmware_id.h"
#include "vboot/util/commonparams.h"

#define FB_MENU_POSITION_COL	26
#define FB_MENU_POSITION_ROW	10
#define FB_MENU_FOREGROUND	10
#define FB_MENU_BACKGROUND	0

#define FB_INFO_POSITION_COL	60
#define FB_INFO_POSITION_ROW	34
#define FB_INFO_FOREGROUND	11
#define FB_INFO_BACKGROUND	0

fb_ret_type __attribute__((weak)) device_mode_enter(void)
{
	return FB_CONTINUE_RECOVERY;
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

	fb_ret_type ret = device_mode_enter();

	switch(ret) {
	case FB_REBOOT:
		/* Does not return */
		cold_reboot();
		break;
	case FB_REBOOT_BOOTLOADER:
		vboot_update_recovery(VBNV_RECOVERY_FW_FASTBOOT);
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
		vboot_update_recovery(VBNV_RECOVERY_RO_FIRMWARE);
		cold_reboot();
		break;
	}
}

static void menu_restart(void)
{
	/* recovery request is automatically cleared by vboot at every boot */
	cold_reboot();
}

static void menu_fastboot(void)
{
	udc_fastboot();
}

static void menu_shutdown(void)
{
	power_off();
}

void vboot_try_fastboot(void)
{
	static const struct {
		const char *text;
		const int fg; /* VGA color */
		const int bg; /* VGA color */
		void (*execute)(void);
		void (*highlight)(void);
	} cmds[] = {
		{ "Restart Android", FB_MENU_FOREGROUND, FB_MENU_BACKGROUND,
				menu_restart, NULL},
		{ "Enter Fastboot", FB_MENU_FOREGROUND, FB_MENU_BACKGROUND,
				menu_fastboot, NULL},
		{ "Self Recovery Mode", FB_MENU_FOREGROUND, FB_MENU_BACKGROUND,
				NULL, NULL},
		{ "Power Off", FB_MENU_FOREGROUND, FB_MENU_BACKGROUND,
				menu_shutdown, NULL},
	};
	const int command_count = ARRAY_SIZE(cmds);
	int pos = 0, row, col;
	unsigned int rows, cols;
	int i;

	if (is_fastboot_mode_requested()) {
		/* fastboot mode is explicitly requested. skip menu. */
		printf("entering fastboot mode\n");
		udc_fastboot();
		return;
	}

	vboot_draw_screen(VB_SCREEN_FASTBOOT_MENU, 0, 1);
	video_get_rows_cols(&rows, &cols);

	for (i = 0; i < command_count; i++) {
		video_console_set_cursor(FB_MENU_POSITION_COL,
					 FB_MENU_POSITION_ROW + i);
		video_printf(cmds[i].fg, cmds[i].bg, 0, cmds[i].text);
	}

	/*
	 * TODO: Show variant name, serial number, signing state
	 */
	row = FB_INFO_POSITION_ROW;
	col = FB_INFO_POSITION_COL;
	video_console_set_cursor(col, row++);
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 0,
		     "PRODUCT NAME: %s %s",
		     cb_mb_vendor_string(lib_sysinfo.mainboard),
		     cb_mb_part_string(lib_sysinfo.mainboard));
	video_console_set_cursor(col, row++);
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 0,
		     "HW VERSION: %d", lib_sysinfo.board_id);
	const char *version = get_active_fw_id();
	if (version) {
		video_console_set_cursor(col, row++);
		video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 0,
			     "BOOTLOADER VERSION: %s", version);
	}
	video_console_set_cursor(col, row++);
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 0,
		     "LOCK STATE: %s",
		     fb_device_unlocked() ? "Unlocked" : "Locked");

	while (1) {
		video_console_set_cursor(FB_MENU_POSITION_COL,
					 FB_MENU_POSITION_ROW + pos);
		video_printf(cmds[pos].bg, cmds[pos].fg, 0, cmds[pos].text);
		if (cmds[pos].highlight)
			cmds[pos].highlight();
		int keypress = getchar();
		printf("got keypress %x\n", keypress);
		if (keypress == ' ') {
			video_console_set_cursor(FB_MENU_POSITION_COL,
						 FB_MENU_POSITION_ROW + pos);
			video_printf(cmds[pos].fg, cmds[pos].bg, 0,
				     cmds[pos].text);
			if (++pos == command_count)
				pos = 0;
		} else if (keypress & KEY_SELECT) {
			printf("selected %s\n", cmds[pos].text);
			if (cmds[pos].execute == NULL)
				/* leave menu and continue to boot */
				break;
			cmds[pos].execute();
			break;
		}
	}
	printf("leaving fastboot menu\n");
}

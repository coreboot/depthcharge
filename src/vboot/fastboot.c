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
#include "boot/bcb.h"
#include "drivers/ec/cros/commands.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/power/power.h"
#include "drivers/video/display.h"
#include "fastboot/fastboot.h"
#include "vboot/stages.h"
#include "vboot/firmware_id.h"
#include "vboot/util/commonparams.h"

#define FB_MENU_POSITION_COL	26
#define FB_MENU_POSITION_ROW	11
#define FB_MENU_FOREGROUND	10
#define FB_MENU_BACKGROUND	0

#define FB_INFO_POSITION_COL	60
#define FB_INFO_POSITION_ROW	34
#define FB_INFO_FOREGROUND	11
#define FB_INFO_BACKGROUND	0
#define FB_WARN_FOREGROUND	11
#define FB_WARN_BACKGROUND	1

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
	 * 2. Recovery reason is set to fastboot requested by User-mode, or
	 * 3. Recovery reason is set to recovery requested by BCB, or
	 * 4. Recovery reason is set to manual request and keyboard mask is set
	 * to fastboot event.
	 *
	 * Else, enter recovery-menu to allow user to choose from different
	 * options.
	 */
	return ((vdat->recovery_reason == VBNV_RECOVERY_FW_FASTBOOT) ||
		(vdat->recovery_reason == VBNV_RECOVERY_US_FASTBOOT) ||
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
	mdelay(100);
	cold_reboot();
}

static void menu_fastboot(void)
{
	udc_fastboot();
}

static void menu_shutdown(void)
{
	mdelay(100);
	power_off();
}

static void menu_recovery(void)
{
	/*
	 * TODO(furquan): Currently we only have support for BCB to reboot into
	 * recovery.
	 */
#if CONFIG_BCB_SUPPORT
	bcb_request_recovery("boot-recovery", "");
#else
	die("menu_recovery not implemented.\n");
#endif
	menu_restart();
}

typedef enum {
	MENU_NONE = 0,
	MENU_NORMAL = (1 << 0),
	MENU_DEV = (1 << 1),
} menu_mode_t;

/*
 * Fastboot menu options
 */
static const struct {
	const char *text;
	const int fg;			/* VGA color for text foreground */
	const int bg;			/* VGA color for text background */
	void (*execute)(void);		/* called when option is executed */
	void (*highlight)(void);	/* called when option is highlighted */
	menu_mode_t mode;		/* mode to display this option */
} opts[] = {
	{"Restart this device", FB_MENU_FOREGROUND, FB_MENU_BACKGROUND,
	 menu_restart, NULL, MENU_NORMAL | MENU_DEV },
	{"Switch to fastboot mode", FB_MENU_FOREGROUND, FB_MENU_BACKGROUND,
	 menu_fastboot, NULL, MENU_NORMAL | MENU_DEV },
	{"Reboot into Android Recovery", FB_MENU_FOREGROUND, FB_MENU_BACKGROUND,
	 menu_recovery, NULL, MENU_NORMAL | MENU_DEV },
	{"Turn off this device", FB_MENU_FOREGROUND, FB_MENU_BACKGROUND,
	 menu_shutdown, NULL, MENU_NORMAL | MENU_DEV },
	{"Switch to USB recovery", FB_MENU_FOREGROUND, FB_MENU_BACKGROUND,
	 NULL, NULL, MENU_NORMAL | MENU_DEV },
};

static void draw_option(int pos, int highlight)
{
	video_console_set_cursor(FB_MENU_POSITION_COL,
				 FB_MENU_POSITION_ROW + pos);
	if (highlight) {
		video_printf(opts[pos].bg, opts[pos].fg, 0, opts[pos].text);
		if (opts[pos].highlight)
			opts[pos].highlight();
	} else {
		video_printf(opts[pos].fg, opts[pos].bg, 0, opts[pos].text);
	}
}

static menu_mode_t vboot_get_menu_mode(void)
{
	if (vboot_in_developer())
		return MENU_DEV;

	return MENU_NORMAL;
}

/* Holds current position that is highlighted in menu. */
static int cur_pos = -1;

static void draw_menu(void)
{
	int i;
	int highlight = 0;
	menu_mode_t mode = vboot_get_menu_mode();

	for (i = 0; i < ARRAY_SIZE(opts); i++) {
		highlight = 0;
		if (opts[i].mode & mode) {
			if (cur_pos == -1) {
				cur_pos = i;
				highlight = 1;
			}
			draw_option(i, highlight);
		}
	}
}

static void menu_move(int incr)
{
	menu_mode_t mode = vboot_get_menu_mode();
	draw_option(cur_pos, 0);

	while (1) {
		cur_pos = (cur_pos + ARRAY_SIZE(opts) + incr) %
			ARRAY_SIZE(opts);
		if (opts[cur_pos].mode & mode) {
			draw_option(cur_pos, 1);
			break;
		}
	}
}

static inline void menu_move_down(void)
{
	menu_move(1);
}

static inline void menu_move_up(void)
{
	menu_move(-1);
}

static inline void menu_select(void)
{
	if (opts[cur_pos].execute)
		opts[cur_pos].execute();

	cur_pos = -1;
}

const char * __attribute__((weak)) board_fw_version(void)
{
	return get_active_fw_id();
}

static void display_banner(void)
{
	VbSharedDataHeader *vdat;
	int size;

	assert(find_common_params((void **)&vdat, &size) == 0);

	if (vdat->recovery_reason == VBNV_RECOVERY_RW_NO_KERNEL)
		video_printf(FB_WARN_FOREGROUND, FB_WARN_BACKGROUND, 0,
			     "  OS on your tablet is damaged. "
			     "Need recovery.  ");
}

static void draw_device_info(void)
{
	const char *version = board_fw_version();
	int row = FB_INFO_POSITION_ROW;

	/*
	 * TODO: Show variant name, serial number, signing state
	 */
	video_console_set_cursor(FB_INFO_POSITION_COL, row++);
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 0,
		     "PRODUCT NAME: %s %s",
		     cb_mb_vendor_string(lib_sysinfo.mainboard),
		     cb_mb_part_string(lib_sysinfo.mainboard));
	video_console_set_cursor(FB_INFO_POSITION_COL, row++);
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 0,
		     "HW VERSION: %d", lib_sysinfo.board_id);
	if (version) {
		video_console_set_cursor(FB_INFO_POSITION_COL, row++);
		video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 0,
			     "BOOTLOADER VERSION: %s", version);
	}
	video_console_set_cursor(FB_INFO_POSITION_COL, row++);
	video_printf(FB_INFO_FOREGROUND, FB_INFO_BACKGROUND, 0,
		     "LOCK STATE: %s",
		     fb_device_unlocked() ? "Unlocked" : "Locked");

	video_console_set_cursor(FB_INFO_POSITION_COL, row+=4);

	display_banner();
}

void vboot_try_fastboot(void)
{
	int loop = 1;

	if (is_fastboot_mode_requested()) {
		/* fastboot mode is explicitly requested. skip menu. */
		printf("entering fastboot mode\n");
		udc_fastboot();
		return;
	}

	vboot_draw_screen(VB_SCREEN_FASTBOOT_MENU, 0, 1);
	draw_menu();
	draw_device_info();

	while (loop) {

		int keypress = board_getchar(FB_BUTTON_DOWN | FB_BUTTON_UP |
					     FB_BUTTON_SELECT);

		switch (keypress) {
		case FB_BUTTON_DOWN:
			menu_move_down();
			break;
		case FB_BUTTON_UP:
			menu_move_up();
			break;
		case FB_BUTTON_SELECT:
			menu_select();
			loop = 0;
			break;
		default:
			printf("Unexpected input\n");
			break;
		}
	}
	printf("leaving fastboot menu\n");
}

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
 */

#ifndef __DRIVERS_VIDEO_DISPLAY_H__
#define __DRIVERS_VIDEO_DISPLAY_H__

/* TODO(b/151200757): Support headless devices */
/* Predefined (default) screens for DisplayOps.display_screen(). */
enum VbScreenType_t {
	VB_SCREEN_BLANK = 0,
	VB_SCREEN_DEVELOPER_WARNING = 0x101,
	VB_SCREEN_RECOVERY_INSERT   = 0x202,
	VB_SCREEN_RECOVERY_NO_GOOD  = 0x203,
	VB_SCREEN_RECOVERY_TO_DEV   = 0x204,
	VB_SCREEN_DEVELOPER_TO_NORM = 0x205,
	VB_SCREEN_WAIT              = 0x206,
	VB_SCREEN_TO_NORM_CONFIRMED = 0x207,
	VB_SCREEN_OS_BROKEN         = 0x208,
	VB_SCREEN_DEVELOPER_WARNING_MENU = 0x20a,
	VB_SCREEN_DEVELOPER_MENU = 0x20b,
	VB_SCREEN_RECOVERY_TO_DEV_MENU = 0x20d,
	VB_SCREEN_DEVELOPER_TO_NORM_MENU = 0x20e,
	VB_SCREEN_LANGUAGES_MENU = 0x20f,
	VB_SCREEN_OPTIONS_MENU = 0x210,
	VB_SCREEN_ALT_FW_PICK = 0x212,
	VB_SCREEN_ALT_FW_MENU = 0x213,
	VB_COMPLETE_VENDOR_DATA = 0x300,
	VB_SCREEN_SET_VENDOR_DATA = 0x301,
	VB_SCREEN_CONFIRM_VENDOR_DATA = 0x302,
	VB_SCREEN_CONFIRM_DIAG = 0x303,
};

typedef struct DisplayOps {
	/**
	 * Perform any work required for initializing the display.
	 *
	 * @param me		DisplayOps struct
	 * @return 0 on success, non-zero on error
	 */
	int (*init)(struct DisplayOps *me);

	/**
	 * Update the backlight according to the enable parameter.
	 *
	 * @param me		DisplayOps struct
	 * @param enable	1 for backlight on, 0 for backlight off
	 * @return 0 on success, non-zero on error
	 */
	int (*backlight_update)(struct DisplayOps *me, uint8_t enable);

	/**
	 * Stop the display in preparation for kernel boot.
	 *
	 * @param me		DisplayOps struct
	 * @return 0 on success, non-zero on error
	 */
	int (*stop)(struct DisplayOps *me);

	/* TODO(b/151200757): Support headless devices */
	/**
	 * Screen display function for headless devices.
	 *
	 * Only on headless devices (CONFIG_HEADLESS=y), this function will be
	 * called instead of the default screen display function.  The board
	 * may implement to indicate to the user via LEDs or sound as to which
	 * screen is being display.
	 *
	 * @param me		DisplayOps struct
	 * @param screen	The screen which should be displayed
	 * 			(see enum VbScreenType_t)
	 * @return 0 on success, non-zero on error
	 */
	int (*display_screen)(struct DisplayOps *me,
			      enum VbScreenType_t screen);
} DisplayOps;

void display_set_ops(DisplayOps *ops);

/*
 * Calls the appropriate function of the current DisplayOps struct.
 * Returns 0 on success or if unimplemented, and non-zero on failure.
 */
int display_init(void);
int backlight_update(uint8_t enable);
int display_screen(enum VbScreenType_t screen);

#endif

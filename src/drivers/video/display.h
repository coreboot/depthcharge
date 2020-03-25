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

#include <vboot_api.h>  /* for VbScreenType_t */

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

/*
 * Copyright 2014 Google LLC
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

#include <stdbool.h>

#include "vboot/ui.h"

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
	int (*backlight_update)(struct DisplayOps *me, bool enable);

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
	 * Only on headless devices (CONFIG(HEADLESS)=y), this function will be
	 * called instead of the default screen display function.  The board
	 * may implement to indicate to the user via LEDs or sound as to which
	 * screen is being display.
	 *
	 * @param me		DisplayOps struct
	 * @param screen	The screen which should be displayed
	 *			(see enum ui_screen)
	 * @return 0 on success, non-zero on error
	 */
	int (*display_screen)(struct DisplayOps *me,
			      enum ui_screen screen);
} DisplayOps;

void display_set_ops(DisplayOps *ops);

/*
 * Calls the appropriate function of the current DisplayOps struct.
 * Returns 0 on success or if unimplemented, and non-zero on failure.
 */
int display_init(void);

/* Returns 1 if init is required (framebuffer is available), otherwise 0. */
int display_init_required(void);

/*
 * Returns 1 if external display is initialized and is ready.
 * Otherwise 0 aka external display not available.
 */
int has_external_display(void);

int backlight_update(bool enable);

int display_screen(enum ui_screen screen);

int display_stop(void);

#endif

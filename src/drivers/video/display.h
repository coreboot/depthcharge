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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __DRIVERS_VIDEO_DISPLAY_H__
#define __DRIVERS_VIDEO_DISPLAY_H__

#include <vboot_api.h>

typedef struct DisplayOps {
	/* Perform any work required for initializing the display. */
	int (*init)(struct DisplayOps *me);
	/* Update the backlight according to the enable parameter. */
	int (*backlight_update)(struct DisplayOps *me, uint8_t enable);
	/* Stop the display as kernel boot is taking place. */
	int (*stop)(struct DisplayOps *me);
	/* Display a screen appropriate for the Vboot state. */
	int (*display_screen)(struct DisplayOps *me,
			      enum VbScreenType_t screen);
} DisplayOps;

int display_init(void);
int backlight_update(uint8_t enable);
void display_set_ops(DisplayOps *ops);
int display_screen(enum VbScreenType_t screen);

#endif

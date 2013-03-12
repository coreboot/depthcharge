/*
 * Copyright 2013 Google Inc.
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */
#ifndef __DRIVERS_INPUT_MKBP_LAYOUT_H__
#define __DRIVERS_INPUT_MKBP_LAYOUT_H__

#include <stdint.h>

enum {
	MkbpLayoutNoMod,
	MkbpLayoutShift,
	MkbpLayoutAlt,
	MkbpLayoutShiftAlt,

	MkbpLayoutMax
};

enum {
	MkbpLayoutSize = 0x57
};

typedef uint16_t MkbpLayout[MkbpLayoutMax][MkbpLayoutSize];

extern MkbpLayout mkbp_keyboard_layout;

#endif /* __DRIVERS_INPUT_MKBP_LAYOUT_H__ */

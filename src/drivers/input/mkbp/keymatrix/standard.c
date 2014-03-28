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

#include "drivers/input/mkbp/keymatrix.h"

enum {
	Rows = 8,
	Cols = 13
};

static uint16_t scancodes[Rows][Cols] = {
//	  0,8     1,9     2,a     3,b     4,c     5       6       7
//-----------------------------------------------------------------------
// 0	|         CPSLCK  F1      B       F10             N
	{ 0xffff, 0x003a, 0x003b, 0x0030, 0x0044, 0xffff, 0x0031, 0xffff,
//	| =               R_ALT
	  0x000d, 0xffff, 0x0064, 0xffff, 0xffff },
// 1	|         ESC     F4      G       F7              H
	{ 0xffff, 0x0001, 0x003e, 0x0022, 0x0041, 0xFFFF, 0x0023, 0xffff,
//	| '       F9              BKSPCE
	  0x0028, 0x0043, 0xffff, 0x000e, 0xffff },
// 2	| L_CTRL  TAB     F3      T       F6      ]       Y       102nd
	{ 0x001d, 0x000f, 0x003d, 0x0014, 0x0040, 0x001b, 0x0015, 0x0056,
//	| [       F8
	  0x001a, 0x0042, 0xffff, 0xffff, 0xffff },
// 3	|         GRAVE   F2      5       F5              6
	{ 0xffff, 0x0029, 0x003c, 0x0006, 0x003f, 0xffff, 0x0007, 0xffff,
//	| -               \_
	  0x000c, 0xffff, 0x002b, 0xffff, 0xffff },
// 4	| R_CTRL  A       D       F       S       K       J
	{ 0x0061, 0x001e, 0x0020, 0x0021, 0x001f, 0x0025, 0x0024, 0xffff,
//	| ;       L               ENTER
	  0x0027, 0x0026, 0xffff, 0x001c, 0xffff },
// 5	|         Z       C       V       X       ,       M       L_SHFT
	{ 0xffff, 0x002c, 0x002e, 0x002f, 0x002d, 0x0033, 0x0032, 0x002a,
//	| /       .               SPACE
	  0x0035, 0x0034, 0xffff, 0x0039, 0xffff },
// 6	|         1       3       4       2       8       7
	{ 0xffff, 0x0002, 0x0004, 0x0005, 0x0003, 0x0009, 0x0008, 0xffff,
//	| 0       9       L_ALT   DOWN    RIGHT
	  0x000b, 0x000a, 0x0038, 0x006c, 0x006a },
// 7	|         Q       E       R       W       I       U       R_SHFT
	{ 0xffff, 0x0010, 0x0012, 0x0013, 0x0011, 0x0017, 0x0016, 0x0036,
//	| P       O               UP      LEFT
	  0x0019, 0x0018, 0xffff, 0x0067, 0x0069 }
};

static uint16_t *scancode_rows[] = {
	scancodes[0], scancodes[1], scancodes[2], scancodes[3],
	scancodes[4], scancodes[5], scancodes[6], scancodes[7]
};

MkbpKeymatrix mkbp_keymatrix = { Rows, Cols, scancode_rows };

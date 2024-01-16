/*
 * Copyright 2013 Google LLC
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
 */
#ifndef __DRIVERS_INPUT_MKBP_KEYMATRIX_H__
#define __DRIVERS_INPUT_MKBP_KEYMATRIX_H__

#include <stddef.h>
#include <stdint.h>

/*
 * These values are just random ones used since MKBP keyboard is supposed to use
 * values lesser than MkbpLayoutSize.
 */
enum {
	MKBP_BUTTON_VOL_DOWN_SHORT_PRESS = 0xe100,
	MKBP_BUTTON_VOL_UP_SHORT_PRESS = 0xe101,
	MKBP_BUTTON_POWER_SHORT_PRESS = 0xe102,
	MKBP_BUTTON_VOL_DOWN_LONG_PRESS = 0xe200,
	MKBP_BUTTON_VOL_UP_LONG_PRESS = 0xe201,
	MKBP_BUTTON_VOL_UP_DOWN_COMBO_PRESS = 0xe300,
	MKBP_BUTTON_INVALID = 0xffff,
};

typedef struct MkbpKeymatrix {
	int rows;
	int cols;
	uint16_t **scancodes;
} MkbpKeymatrix;

typedef struct MkbpButtonInfo {
	uint32_t button_combo;
	uint16_t short_press_code;
	uint16_t long_press_code;
} MkbpButtonInfo;

extern MkbpKeymatrix mkbp_keymatrix;
extern MkbpButtonInfo mkbp_buttoninfo[];
extern size_t mkbp_buttoninfo_count;

#endif /* __DRIVERS_INPUT_MKBP_KEYMATRIX_H__ */

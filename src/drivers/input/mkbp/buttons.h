/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2024 Google LLC.  */

#ifndef __DRIVERS_INPUT_MKBP_BUTTONS_H__
#define __DRIVERS_INPUT_MKBP_BUTTONS_H__

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

typedef struct MkbpButtonInfo {
	uint32_t button_combo;
	uint16_t short_press_code;
	uint16_t long_press_code;
} MkbpButtonInfo;

extern MkbpButtonInfo mkbp_buttoninfo[];
extern size_t mkbp_buttoninfo_count;

#endif /* __DRIVERS_INPUT_MKBP_BUTTONS_H__ */

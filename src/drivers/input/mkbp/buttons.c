// SPDX-License-Identifier: GPL-2.0
/* Copyright 2024 Google LLC.  */

#include <libpayload.h>

#include "drivers/input/mkbp/buttons.h"
#include "drivers/ec/cros/commands.h"

#define MKBP_BUTTON(x)		(1 << (x))

MkbpButtonInfo mkbp_buttoninfo[] = {
	{
		.button_combo = MKBP_BUTTON(EC_MKBP_POWER_BUTTON),
		.short_press_code = MKBP_BUTTON_POWER_SHORT_PRESS,
		.long_press_code = MKBP_BUTTON_INVALID,
	},
	{
		.button_combo = MKBP_BUTTON(EC_MKBP_VOL_UP),
		.short_press_code = MKBP_BUTTON_VOL_UP_SHORT_PRESS,
		.long_press_code = MKBP_BUTTON_VOL_UP_LONG_PRESS,
	},
	{
		.button_combo = MKBP_BUTTON(EC_MKBP_VOL_DOWN),
		.short_press_code = MKBP_BUTTON_VOL_DOWN_SHORT_PRESS,
		.long_press_code = MKBP_BUTTON_VOL_DOWN_LONG_PRESS,
	},
	{
		.button_combo = MKBP_BUTTON(EC_MKBP_VOL_UP) |
		MKBP_BUTTON(EC_MKBP_VOL_DOWN),
		.short_press_code = MKBP_BUTTON_VOL_UP_DOWN_COMBO_PRESS,
		.long_press_code = MKBP_BUTTON_INVALID,
	},
};

size_t mkbp_buttoninfo_count = ARRAY_SIZE(mkbp_buttoninfo);

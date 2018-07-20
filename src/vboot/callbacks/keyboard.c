/*
 * Copyright 2012 Google Inc.
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

#include <libpayload.h>
#include <config.h>
#include <vboot_api.h>
#include <vboot/util/flag.h>

#include "debug/dev.h"
#include "drivers/ec/cros/ec.h"

#define CSI_0 0x1B
#define CSI_1 0x5B

#define KEY_DOWN 0402
#define KEY_UP 0403
#define KEY_LEFT 0404
#define KEY_RIGHT 0405

#define BUTTON_VOL_DOWN 0x63
#define BUTTON_VOL_UP   0x62
#define BUTTON_POWER    0x90

#define TIMEOUT_US (10 * 1000)	// 10ms

#define KEYBOARD_COL_KEY_A 1
#define KEYBOARD_ROW_KEY_A 4
#define KEYBOARD_MASK_KEY_A (1 << KEYBOARD_ROW_KEY_A)
#define KEYBOARD_COL_KEY_REFRESH 2
#define KEYBOARD_ROW_KEY_REFRESH 2
#define KEYBOARD_MASK_KEY_REFRESH (1 << KEYBOARD_ROW_KEY_REFRESH)
#define KEYBOARD_COL_KEY_REFRESH_3 2
#define KEYBOARD_ROW_KEY_REFRESH_3 3
#define KEYBOARD_MASK_KEY_REFRESH_3 (1 << KEYBOARD_ROW_KEY_REFRESH_3)

uint32_t VbExKeyboardRead(void)
{
	uint64_t timer_start;

	// No input, just give up.
	if (!havechar())
		return 0;

	uint32_t ch = getchar();
	switch (ch) {
	case '\n': return '\r';
	case KEY_UP: return VB_KEY_UP;
	case KEY_DOWN: return VB_KEY_DOWN;
	case KEY_RIGHT: return VB_KEY_RIGHT;
	case KEY_LEFT: return VB_KEY_LEFT;
	case BUTTON_VOL_DOWN: return VB_BUTTON_VOL_DOWN;
	case BUTTON_VOL_UP: return VB_BUTTON_VOL_UP;
	case BUTTON_POWER: return VB_BUTTON_POWER;
	case CSI_0:
		timer_start = timer_us(0);
		while (!havechar()) {
			if (timer_us(timer_start) >= TIMEOUT_US)
				return CSI_0;
		}

		// Ignore non escape [ sequences.
		if (getchar() != CSI_1)
			return CSI_0;

		// Translate the arrow keys, and ignore everything else.
		switch (getchar()) {
		case 'A': return VB_KEY_UP;
		case 'B': return VB_KEY_DOWN;
		case 'C': return VB_KEY_RIGHT;
		case 'D': return VB_KEY_LEFT;
		default: return 0;
		}

	// These two cases only work on developer images (empty stubs otherwise)
	case 'N' & 0x1f: dc_dev_netboot();	// CTRL+N: netboot
	case 'G' & 0x1f: dc_dev_gdb_enter();	// CTRL+G: remote GDB mode
	// fall through for non-developer images as if these didn't exist
	default:
		return ch;
	}
}

uint32_t VbExKeyboardReadWithFlags(uint32_t *flags_ptr)
{
	uint32_t c = VbExKeyboardRead();
	if (flags_ptr) {
		*flags_ptr = 0;
		// USB keyboards definitely cannot be trusted (assuming they
		// are even keyboards).  There are other devices that also
		// cannot be trusted, but this is the best we can do for now.
		if (last_key_input_type() != CONSOLE_INPUT_TYPE_USB)
			*flags_ptr |= VB_KEY_FLAG_TRUSTED_KEYBOARD;
	}
	return c;
}

int vb2ex_get_alt_os_hotkey(void)
{
	uint8_t key_matrix[CROS_EC_KEYSCAN_COLS];
	int ret = cros_ec_keyboard_get_boot_time_matrix(
			key_matrix, sizeof(key_matrix));
	if (ret != 0)
		return 0;

	printf("Key_matrix: ");
	for (int i = 0; i < CROS_EC_KEYSCAN_COLS; i++)
		printf("%02hhx, ", key_matrix[i]);
	printf("\n");

	if (!(key_matrix[KEYBOARD_COL_KEY_A] & KEYBOARD_MASK_KEY_A))
		return 0;

	/*
	 * Clear key A and refresh (possibly on row 2 or row 3),
	 * reject if any other key is pressed.
	 */
	key_matrix[KEYBOARD_COL_KEY_A] &= ~KEYBOARD_MASK_KEY_A;
	key_matrix[KEYBOARD_COL_KEY_REFRESH] &= ~KEYBOARD_MASK_KEY_REFRESH;
	key_matrix[KEYBOARD_COL_KEY_REFRESH_3] &= ~KEYBOARD_MASK_KEY_REFRESH_3;
	for (int i = 0; i < CROS_EC_KEYSCAN_COLS; i++)
		if (key_matrix[i])
			return 0;

	/* Reset the key matrix only if this is our key combo. */
	cros_ec_keyboard_clear_boot_time_matrix();
	return 1;
}

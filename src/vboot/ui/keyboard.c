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
#include <vboot/ui.h>
#include <vboot/util/flag.h>

#include "debug/dev.h"

#define CSI_0 0x1B
#define CSI_1 0x5B

#define KEY_DOWN 0402
#define KEY_UP 0403
#define KEY_LEFT 0404
#define KEY_RIGHT 0405

#define TIMEOUT_US (10 * 1000)	// 10ms

static uint32_t keyboard_get_key(void)
{
	uint64_t timer_start;

	// No input, just give up.
	if (!havechar())
		return 0;

	uint32_t ch = getchar();
	switch (ch) {
	case '\n': return '\r';
	case KEY_UP: return UI_KEY_UP;
	case KEY_DOWN: return UI_KEY_DOWN;
	case KEY_RIGHT: return UI_KEY_RIGHT;
	case KEY_LEFT: return UI_KEY_LEFT;

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
		case 'A': return UI_KEY_UP;
		case 'B': return UI_KEY_DOWN;
		case 'C': return UI_KEY_RIGHT;
		case 'D': return UI_KEY_LEFT;
		default: return 0;
		}

	// These two cases only work on developer images (empty stubs otherwise)
	case UI_KEY_CTRL('N'):
		dc_dev_netboot();	// CTRL+N: netboot
		__attribute__((fallthrough));
	case UI_KEY_CTRL('G'):
		dc_dev_gdb_enter();	// CTRL+G: remote GDB mode
		__attribute__((fallthrough));
	case UI_KEY_CTRL('F'):
		dc_dev_fastboot();	// CTRL+F: fastboot
		__attribute__((fallthrough));
	// fall through for non-developer images as if these didn't exist
	default:
		return ch;
	}
}

uint32_t ui_keyboard_read(uint32_t *flags_ptr)
{
	uint32_t c = keyboard_get_key();

	if (!flags_ptr)
		return c;

	*flags_ptr = 0;
	console_input_type input_type = last_key_input_type();
	// Always trust UART and GPIO keyboards, and only trust
	// EC-based keyboards when coming from a trusted EC.
	// All other keyboards, including USB, are untrusted.
	if (input_type == CONSOLE_INPUT_TYPE_UART ||
	    input_type == CONSOLE_INPUT_TYPE_GPIO ||
	    (CONFIG_DRIVER_EC_CROS &&
	     input_type == CONSOLE_INPUT_TYPE_EC))
		*flags_ptr |= UI_KEY_FLAG_TRUSTED_KEYBOARD;
	return c;
}

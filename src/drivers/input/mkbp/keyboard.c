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
 */

#include <assert.h>
#include <keycodes.h>
#include <libpayload.h>
#include <stdint.h>

#include "base/init_funcs.h"
#include "config.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/input/input.h"
#include "drivers/input/mkbp/keymatrix.h"
#include "drivers/input/mkbp/layout.h"

typedef enum Modifier {
	ModifierNone = 0x0,
	ModifierCtrl = 0x1,
	ModifierAlt = 0x2,
	ModifierShift = 0x4
} Modifier;

/*
 * These values come from libpayload/drivers/keyboard.c.  Vol Down/Up ascii
 * values were already defined for 8042.  Power button ascii value was created
 * for detachables.  0x90 was picked because it was out of the matrix keyboard
 * range to avoid conflicts.
 */
typedef enum Buttons {
	BUTTON_VOL_DOWN = 0x62,
	BUTTON_VOL_UP = 0x63,
	BUTTON_POWER = 0x90
} Buttons;

/*
 * EC has no more states if:
 * 1. It no longer asserts the interrupt line or
 * 2. EC_HOST_EVENT_MKBP is not set if interrupt line is not supported.
 */
static int more_input_states(void)
{
	if (IS_ENABLED(CONFIG_DRIVER_INPUT_MKBP_NO_INTERRUPT)) {
		uint32_t events;
		const uint32_t mkbp_mask =
			EC_HOST_EVENT_MASK(EC_HOST_EVENT_MKBP);

		if ((cros_ec_get_host_events(&events) == 0) &&
			(events & mkbp_mask)) {
			cros_ec_clear_host_events(mkbp_mask);
			return 1;
		}

		return 0;
	}

	return cros_ec_interrupt_pending();
}

// Returns amount of scanned keys, or -1 if EC's buffer is known to be empty.
static int read_scancodes(Modifier *modifiers, uint16_t *codes, int max_codes)
{
	static struct cros_ec_keyscan last_scan;
	static struct ec_response_get_next_event event;
	static struct cros_ec_keyscan scan;

	assert(modifiers);
	*modifiers = ModifierNone;

	if (!more_input_states())
		return -1;

	if (IS_ENABLED(CONFIG_DRIVER_INPUT_MKBP_OLD_COMMAND)) {
		if (cros_ec_scan_keyboard(&scan)) {
			printf("Key matrix scan failed.\n");
			return -1;
		}

		memcpy(&event.data.key_matrix, &scan.data, sizeof(scan.data));
	} else {
		// Get pending MKBP event.  It may not be a key matrix event.
		do {
			int rv = cros_ec_get_next_event(&event);
			// The EC has no events for us at this time.
			if (rv == -EC_RES_UNAVAILABLE) {
				return -1;
			} else if (rv < 0) {
				printf("Error getting next MKBP event. (%d)\n",
				       rv);
				return -1;
			}
		} while (event.event_type != EC_MKBP_EVENT_KEY_MATRIX &&
			 event.event_type != EC_MKBP_EVENT_BUTTON);
	}

	int total = 0;
	int changed = 0;

	int cols = mkbp_keymatrix.cols;
	int rows = mkbp_keymatrix.rows;
	int num_keys = cols * rows;

	typedef struct Key
	{
		uint8_t row;
		uint8_t col;
		uint16_t code;
	} Key;

	Key keys[num_keys];

	/* 1.  Check for button events first
	 *
	 * Assuming that scancodes are in the same order as the bit order of the
	 * map of EC_MKBP_* events defined in
	 * depthcharge/src/ec/cros/commands.h.  For example, as
	 * EC_MKBP_POWER_BUTTON=0, then the scancode for this button will be
	 * first in the button_scancodes array.  EC_MKBP_VOL_UP=1, so its
	 * scancode is the 2nd entry in the button_scancode array.
	 */
	if (event.event_type == EC_MKBP_EVENT_BUTTON) {
		uint32_t last_buttons = last_scan.buttons;
		uint32_t buttons = event.data.buttons;
		last_scan.buttons = buttons;

		uint32_t pressed_buttons = (last_buttons ^ buttons) & buttons;
		if (pressed_buttons & (1 << EC_MKBP_POWER_BUTTON)) {
			keys[total].code =
			  mkbp_keymatrix.button_scancodes[EC_MKBP_POWER_BUTTON];
			keys[total].row = 0xFF;
			keys[total].col = 0xFF;
			total++;
			changed++;
		}
		if (pressed_buttons & (1 << EC_MKBP_VOL_UP)) {
			keys[total].code =
			  mkbp_keymatrix.button_scancodes[EC_MKBP_VOL_UP];
			keys[total].row = 0xFF;
			keys[total].col = 0xFF;
			total++;
			changed++;
		}
		if (pressed_buttons & (1 << EC_MKBP_VOL_DOWN)) {
			keys[total].code =
			  mkbp_keymatrix.button_scancodes[EC_MKBP_VOL_DOWN];
			keys[total].row = 0xFF;
			keys[total].col = 0xFF;
			total++;
			changed++;
		}
	}

	/* 2.  Now check for keyboard matrix events */
	if (event.event_type == EC_MKBP_EVENT_KEY_MATRIX) {
		for (int pos = 0; pos < num_keys; pos += 8) {
			int byte = pos / 8;

			uint8_t last_data = last_scan.data[byte];
			uint8_t data = event.data.key_matrix[byte];
			last_scan.data[byte] = data;

			if (last_data != data)
				changed++;

			// Only a few bits are going to be set at any one time.
			if (!data)
				continue;

			const int max = MIN(8, num_keys - pos);
			for (int i = 0; i < max; i++) {
				if ((data >> i) & 0x1) {
					int row = (pos + i) % rows;
					int col = (pos + i) / rows;

					uint16_t code =
					     mkbp_keymatrix.scancodes[row][col];

					/*
					 * Look for modifiers, ignoring
					 * capslock.
					 */
					if (code == 0x1d || code == 0x61)
						*modifiers |= ModifierCtrl;
					if (code == 0x38 || code == 0x64)
						*modifiers |= ModifierAlt;
					if (code == 0x2a || code == 0x36)
						*modifiers |= ModifierShift;

					/*
					 * Ignore keys that were already
					 * pressed.
					 */
					if ((last_data >> i) & 0x1)
						code = 0xffff;

					keys[total].row = row;
					keys[total].col = col;
					keys[total].code = code;
					total++;
					if (total == max_codes)
						break;
				}
			}
			if (total == max_codes)
				break;
		}
	}

	// The EC only resends the same state if its FIFO was empty.
	if (!changed)
		return -1;

	// If there could be ghosting, throw everything away. Also, transfer
	// valid keycodes into the buffer.
	for (int i = 0; i < total; i++) {
		int row_match = 0;
		int col_match = 0;


		if (keys[i].row == 0xFF)
			goto post_ghost; /* Not a matrixed key */

		for (int j = 0; j < total; j++) {
			if (i == j)
				continue;
			if (keys[i].row == keys[j].row) {
				row_match = 1;
				if (col_match)
					return 0;
			}
			if (keys[i].col == keys[j].col) {
				col_match = 1;
				if (row_match)
					return 0;
			}
		}

post_ghost:
		if (keys[i].code != 0xffff)
			*codes++ = keys[i].code;
	}

	return total;
}

enum {
	KeyFifoSize = 16
};

uint16_t key_fifo[KeyFifoSize];
int fifo_offset;
int fifo_size;

static void add_key(uint16_t key)
{
	// Don't do anything if there isn't enough space.
	if (fifo_size == ARRAY_SIZE(key_fifo))
		return;

	key_fifo[fifo_size++] = key;
}

static void more_keys(void)
{
	// No more keys until you finish the ones you've got.
	if (fifo_offset < fifo_size)
		return;

	// FIFO empty, reinitialize it back to its default state.
	fifo_offset = fifo_size = 0;

	// Get scancodes from the EC.
	uint16_t scancodes[KeyFifoSize];
	Modifier modifiers;

	// Keep searching through states until we find a valid key press.
	while (!fifo_size) {
		int count = read_scancodes(&modifiers, scancodes, KeyFifoSize);
		if (count < 0)
			return;	// EC has no more key states buffered.

		// Figure out which layout to use based on the modifiers.
		int map;
		if (modifiers & ModifierAlt) {
			if (modifiers & ModifierShift)
				map = MkbpLayoutShiftAlt;
			else
				map = MkbpLayoutAlt;
		} else if (modifiers & ModifierShift) {
			map = MkbpLayoutShift;
		} else {
			map = MkbpLayoutNoMod;
		}

		// Look at all the keys and fill the FIFO.
		for (int pos = 0; pos < count; pos++) {
			uint16_t code = scancodes[pos];

			// Handle arrow keys.
			if (code == 0x6c)
				add_key(KEY_DOWN);
			else if (code == 0x6a)
				add_key(KEY_RIGHT);
			else if (code == 0x67)
				add_key(KEY_UP);
			else if (code == 0x69)
				add_key(KEY_LEFT);
			else if (code == 0xe021)
				add_key(BUTTON_VOL_DOWN);
			else if (code == 0xe032)
				add_key(BUTTON_VOL_UP);
			else if (code == 0xe037)
				add_key(BUTTON_POWER);

			// Make sure the next check will prevent us from
			// recognizing this key twice.
			assert(MkbpLayoutSize < 0x6c);

			// Ignore the scancode if it's a modifier or too big.
			if (code == 0x1d || code == 0x61 ||
					code == 0x38 || code == 0x64 ||
					code == 0x2a || code == 0x36 ||
					code >= MkbpLayoutSize)
				continue;

			// Map it to its ASCII value.
			uint16_t ascii = mkbp_keyboard_layout[map][code];

			// Handle the Ctrl modifier.
			if ((modifiers & ModifierCtrl) &&
					((ascii >= 'a' && ascii <= 'z') ||
					 (ascii >= 'A' && ascii <= 'Z')))
				ascii &= 0x1f;

			add_key(ascii);
		}
	}
}

static int mkbp_keyboard_havekey(void)
{
	// Get more keys if we need them.
	more_keys();

	return fifo_offset < fifo_size;
}

static int mkbp_keyboard_getchar(void)
{
	while (!mkbp_keyboard_havekey());

	return key_fifo[fifo_offset++];
}

static struct console_input_driver mkbp_keyboard =
{
	NULL,
	&mkbp_keyboard_havekey,
	&mkbp_keyboard_getchar
};

static void mkbp_keyboard_init(void)
{
	// Flush out EC's FIFO to avoid misinterpreting keys from before reboot.
	while (mkbp_keyboard_havekey())
		printf("mkbp: ignoring key code %d received before boot.\n",
		       mkbp_keyboard_getchar());

	console_add_input_driver(&mkbp_keyboard);
}

static int dc_mkbp_install_on_demand_input(void)
{
	static OnDemandInput dev =
	{
		&mkbp_keyboard_init,
		1
	};

	list_insert_after(&dev.list_node, &on_demand_input_devices);
	return 0;
}

INIT_FUNC(dc_mkbp_install_on_demand_input);

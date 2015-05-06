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

#include <assert.h>
#include <keycodes.h>
#include <libpayload.h>
#include <stdint.h>

#include "base/init_funcs.h"
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

static int read_scancodes(Modifier *modifiers, uint8_t *codes, int max_codes)
{
	static struct cros_ec_keyscan last_scan;
	static struct cros_ec_keyscan scan;

	assert(modifiers);
	*modifiers = ModifierNone;

	if (cros_ec_scan_keyboard(&scan)) {
		printf("Key matrix scan failed.\n");
		return 0;
	}

	int total = 0;

	int cols = mkbp_keymatrix.cols;
	int rows = mkbp_keymatrix.rows;
	int num_keys = cols * rows;

	typedef struct Key
	{
		uint8_t row;
		uint8_t col;
		uint8_t code;
	} Key;

	Key keys[num_keys];

	for (int pos = 0; pos < num_keys; pos += 8) {
		int byte = pos / 8;

		uint8_t last_data = last_scan.data[byte];
		uint8_t data = scan.data[byte];
		last_scan.data[byte] = data;

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

				// Look for modifiers, ignoring capslock.
				if (code == 0x1d || code == 0x61)
					*modifiers |= ModifierCtrl;
				if (code == 0x38 || code == 0x64)
					*modifiers |= ModifierAlt;
				if (code == 0x2a || code == 0x36)
					*modifiers |= ModifierShift;

				// Ignore keys that were already pressed.
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

	// If there could be ghosting, throw everything away. Also, transfer
	// valid keycodes into the buffer.
	for (int i = 0; i < total; i++) {
		int row_match = 0;
		int col_match = 0;

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

	// If the EC doesn't assert its interrupt line, it has no more keys.
	if (!cros_ec_interrupt_pending())
		return;

	// Get scancodes from the EC.
	uint8_t scancodes[KeyFifoSize];
	Modifier modifiers;
	int count = read_scancodes(&modifiers, scancodes, KeyFifoSize);

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
		uint8_t code = scancodes[pos];

		// Handle arrow keys.
		if (code == 0x6c)
			add_key(KEY_DOWN);
		else if (code == 0x6a)
			add_key(KEY_RIGHT);
		else if (code == 0x67)
			add_key(KEY_UP);
		else if (code == 0x69)
			add_key(KEY_LEFT);

		// Make sure the next check will prevent us from recognizing
		// this key twice.
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

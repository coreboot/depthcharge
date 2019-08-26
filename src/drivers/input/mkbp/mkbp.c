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
#include <stddef.h>
#include <stdint.h>
#include <vboot_api.h>

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
 * EC has no more states if:
 * 1. It no longer asserts the interrupt line or
 * 2. EC_HOST_EVENT_MKBP is not set if interrupt line is not supported.
 */
static int more_input_states(void)
{
	int interrupt = cros_ec_interrupt_pending();

	if (interrupt >= 0)
		return interrupt;

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

typedef struct Key
{
	uint8_t row;
	uint8_t col;
	uint16_t code;
} Key;

static int mkbp_old_cmd_read_event(struct ec_response_get_next_event *event)
{
	struct cros_ec_keyscan scan;

	if (cros_ec_scan_keyboard(&scan)) {
		printf("Key matrix scan failed.\n");
		return -1;
	}

	event->event_type = EC_MKBP_EVENT_KEY_MATRIX;
	memcpy(&event->data.key_matrix, &scan.data, sizeof(scan.data));
	return 0;
}

static int mkbp_new_cmd_read_event(struct ec_response_get_next_event *event)
{
	int rv;

	while (1) {
		rv = cros_ec_get_next_event(event);
		if (rv == -EC_RES_UNAVAILABLE)
			return -1;

		if (rv < 0) {
			printf("Error getting next MKBP event (%d)\n", rv);
			return -1;
		}

		if ((event->event_type == EC_MKBP_EVENT_KEY_MATRIX) ||
		    (event->event_type == EC_MKBP_EVENT_BUTTON))
			return 0;
	}
}

/*
 * Read event data from EC using the right MKBP command.
 * Return value:
 * 0 = success (more data available in event response)
 * -1 = error (no more data queued on EC side)
 */
static int mkbp_read_event(struct ec_response_get_next_event *event)
{
	if (IS_ENABLED(CONFIG_DRIVER_INPUT_MKBP_OLD_COMMAND))
		return mkbp_old_cmd_read_event(event);

	return mkbp_new_cmd_read_event(event);
}

static int update_keys(Key *keys, int *total, int max,
			uint8_t row, uint8_t col, uint16_t code)
{
	int curr = *total;

	if (curr >= max)
		return -1;

	keys[curr].row = row;
	keys[curr].col = col;
	keys[curr].code = code;

	*total = curr + 1;
	return 0;
}

static int mkbp_process_keymatrix(Modifier *modifiers, uint16_t *codes,
				  int max_codes,
				  struct ec_response_get_next_event *event)
{
	static struct cros_ec_keyscan last_scan;
	int total = 0;
	int changed = 0;
	int cols = mkbp_keymatrix.cols;
	int rows = mkbp_keymatrix.rows;
	int num_keys = cols * rows;
	Key keys[num_keys];

	for (int pos = 0; pos < num_keys; pos += 8) {
		int byte = pos / 8;

		uint8_t last_data = last_scan.data[byte];
		uint8_t data = event->data.key_matrix[byte];
		last_scan.data[byte] = data;

		if (last_data != data)
			changed = 1;

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

				if (update_keys(keys, &total, max_codes,
						row, col, code))
					break;
			}
		}
		if (total == max_codes)
			break;
	}

	// The EC only resends the same state if its FIFO was empty.
	if (!changed)
		return -1;

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

/*
 * Based on the long/short press event, add button scancode using input
 * parameter bitmap and match it against the button_combo in mkbp_buttoninfo.
 *
 * Return value:
 * 0 = button not added (because the button has no valid code for short/long
 * press)
 * 1 = button code and modifier added.
 */
static int mkbp_add_button_code(uint16_t *codes, int max_codes, int *curr,
				 int bitmap, int is_long_press)
{
	size_t i;
	int count = *curr;

	if (count >= max_codes)
		return 0;

	for (i = 0; i < mkbp_buttoninfo_count; i++)
		if (mkbp_buttoninfo[i].button_combo == bitmap)
			break;

	if (i == mkbp_buttoninfo_count)
		return 0;

	if (is_long_press)
		codes[count] = mkbp_buttoninfo[i].long_press_code;
	else
		codes[count] = mkbp_buttoninfo[i].short_press_code;

	*curr = count + 1;

	return 1;
}

/*
 * prev_bitmap stores button bitmap obtained from EC during previous scan. This
 * is required to identify which buttons changed state (press->release) or which
 * buttons have satisfied long press timeout.
 */
static uint32_t prev_bitmap;

/*
 * This function is used to determine if a new button scancode should be
 * added. Rules for determing whether to add a new button:
 * 1. If a single button is pressed, then button scancode is reported on button
 * release and not button press (i.e. release of button is considered as action
 * complete).
 * 2. If more than one button is pressed at a time, report press of the two
 * button combo right away without waiting for release. If the button combo is
 * valid as per mkbp_buttoninfo, then it will be reported to the caller. Else,
 * the combo would be silently ignored.
 * 3. If single button is pressed for more than 3 seconds then it is identified
 * as a long press. Depending upon the mkbp_buttoninfo, different scancode might
 * be reported for long press of buttons.
 */
static int mkbp_add_button(uint16_t *codes, int max_codes, uint32_t curr_bitmap)
{
	const uint64_t long_press_timeout_us = 3 * 1000 * 1000;
	static uint64_t prev_timestamp_us;
	static int combo_detected = 0;
	int total = 0;

	/*
	 * If no buttons were pressed before and no buttons are pressed now,
	 * there is nothing to be done.
	 */
	if (!prev_bitmap && !curr_bitmap)
		return total;

	/* If a button was pressed before, check if it has been released now. */
	if (prev_bitmap & ~curr_bitmap) {
		if (combo_detected) {
			/*
			 * We're still waiting for a combo release... just check
			 * if we're done with that and otherwise ignore.
			 */
			if (!curr_bitmap)
				combo_detected = 0;
		} else {
			/* Normal button release -> add key code. */
			mkbp_add_button_code(codes, max_codes, &total,
					     prev_bitmap, 0);
		}
	} else if (prev_bitmap == curr_bitmap) {
		/*
		 * If a button was pressed before and it is still pressed, then
		 * check if it has been held down for long enough to consider as
		 * long press. If yes, try adding the scancode for long press of
		 * the button (if it exists). If long press scancode does not
		 * exist, just continue.
		 */
		if (timer_us(prev_timestamp_us) >= long_press_timeout_us) {
			if (mkbp_add_button_code(codes, max_codes, &total,
						 prev_bitmap, 1)) {
				prev_bitmap = 0;
				prev_timestamp_us = 0;
			}
		}
		return total;
	}

	/*
	 * If more than one button is pressed, report the combo right away and
	 * set the flag to make sure we ignore the buttons on the way back up.
	 */
	if (curr_bitmap & (curr_bitmap - 1)) {
		mkbp_add_button_code(codes, max_codes, &total, curr_bitmap, 0);
		combo_detected = 1;
	} else {
		prev_bitmap = curr_bitmap;
		prev_timestamp_us = timer_us(0);
	}

	return total;
}

static int mkbp_process_buttons(uint16_t *codes, int max_codes,
				struct ec_response_get_next_event *event)
{
	return mkbp_add_button(codes, max_codes, event->data.buttons);
}

static int mkbp_process_button_long_press(uint16_t *codes, int max_codes)
{
	if (!mkbp_add_button(codes, max_codes, prev_bitmap))
		return -1;

	return 1;
}

static int mkbp_process_events(Modifier *modifiers, uint16_t *codes,
				int max_codes)
{
	struct ec_response_get_next_event event;

	if (!more_input_states())
		return -1;

	if (mkbp_read_event(&event))
		return -1;

	if (event.event_type == EC_MKBP_EVENT_KEY_MATRIX)
		return mkbp_process_keymatrix(modifiers, codes, max_codes,
						&event);
	else if (event.event_type == EC_MKBP_EVENT_BUTTON)
		return mkbp_process_buttons(codes, max_codes, &event);

	return 0;
}

// Returns amount of scanned keys, or -1 if EC's buffer is known to be empty.
static int read_scancodes(Modifier *modifiers, uint16_t *codes, int max_codes)
{
	int total;

	assert(modifiers && codes && max_codes);
	*modifiers = ModifierNone;

	/*
	 * First process MKBP events to determine if there is a new scancode
	 * present.
	 */
	total = mkbp_process_events(modifiers, codes, max_codes);

	/*
	 * If no new scancodes are present, then check if any button has
	 * satisfied long press timeout. If so report its long press scancode.
	 */
	if (total == -1)
		total = mkbp_process_button_long_press(codes, max_codes);

	return total;
}

enum {
	KeyFifoSize = 16
};

static uint16_t key_fifo[KeyFifoSize];
static int fifo_offset;
static int fifo_size;

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
			else if (code == VOL_DOWN_SHORT_PRESS)
				add_key(VB_BUTTON_VOL_DOWN_SHORT_PRESS);
			else if (code == VOL_UP_SHORT_PRESS)
				add_key(VB_BUTTON_VOL_UP_SHORT_PRESS);
			else if (code == POWER_SHORT_PRESS)
				add_key(VB_BUTTON_POWER_SHORT_PRESS);
			else if (code == VOL_DOWN_LONG_PRESS)
				add_key(VB_BUTTON_VOL_DOWN_LONG_PRESS);
			else if (code == VOL_UP_LONG_PRESS)
				add_key(VB_BUTTON_VOL_UP_LONG_PRESS);
			else if (code == VOL_UP_DOWN_COMBO_PRESS)
				add_key(VB_BUTTON_VOL_UP_DOWN_COMBO_PRESS);

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

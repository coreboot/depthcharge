/*
 * Copyright 2014 Google Inc.
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
#include "drivers/input/input.h"
#include "drivers/input/pseudo/keyboard.h"

/*
 * Key codes are expected as follows:
 * ASCII characters : ASCII values (0 - 127)
 * Key UP          : 128
 * Key DOWN        : 129
 * Key Right       : 130
 * Key Left        : 131
 */
size_t __attribute__((weak)) read_key_codes(Modifier *modifiers,
					    uint16_t *codes, size_t max_codes)
{
	return 0;
}

#define KEY_FIFO_SIZE 16
static uint16_t key_fifo[KEY_FIFO_SIZE];
/* Elements are added at the head and removed from the tail */
static size_t fifo_tail;
static size_t fifo_head;

/* Gives # of unused slots in fifo to put elements */
static inline size_t key_fifo_size(void)
{
	return ARRAY_SIZE(key_fifo) - fifo_head;
}

/* Gives # of used unread slots in fifo */
static inline size_t key_fifo_occupied(void)
{
	return fifo_head - fifo_tail;
}

/* Tells if all slots in fifo are used */
static inline int key_fifo_full(void)
{
	return !key_fifo_size();
}

static void key_fifo_put(uint16_t key)
{
	if (key_fifo_full()) {
		printf("%s: dropped a character\n",__func__);
		return;
	}

	key_fifo[fifo_head++] = key;
}

static uint16_t key_fifo_get(void)
{
	assert(key_fifo_occupied());

	uint16_t key = key_fifo[fifo_tail++];
	return key;
}

static void key_fifo_clear(void)
{
	fifo_tail = fifo_head = 0;
}

static void pk_more_keys(void)
{
	/* No more keys until you finish the ones you've got. */
	if (key_fifo_occupied())
		return;

	key_fifo_clear();

	/* Get ascii codes */
	uint16_t key_codes[KEY_FIFO_SIZE];
	Modifier modifiers = MODIFIER_NONE;
	/*
	 * Every board that uses pseudo keyboard is expected to implement its
	 * own read_key_codes since input methods and input pins can vary.
	 */
	size_t count = read_key_codes(&modifiers, key_codes, KEY_FIFO_SIZE);

	assert (count <= KEY_FIFO_SIZE);

	/* Look at all the keys and fill the FIFO. */
	for (size_t pos = 0; pos < count; pos++) {
		uint16_t code = key_codes[pos];

		/* Check for valid keycode */
		if ((code < KEY_CODE_START) || (code > KEY_CODE_END))
			continue;

		/* Check for ascii values of alphabets */
		if (isalpha(code)) {
			/* Convert alpha characters into control characters */
			if (modifiers & MODIFIER_CTRL)
				code &= 0x1f;
			key_fifo_put(code);
			continue;
		}

		/* Handle special keys */
		switch (code) {
		case KEY_CODE_UP:
			key_fifo_put(KEY_UP);
			break;
		case KEY_CODE_DOWN:
			key_fifo_put(KEY_DOWN);
			break;
		case KEY_CODE_RIGHT:
			key_fifo_put(KEY_RIGHT);
			break;
		case KEY_CODE_LEFT:
			key_fifo_put(KEY_LEFT);
			break;
		default:
			key_fifo_put(code);
		}
	}
}

static int pk_havekey(void)
{
	/* Get more keys if we need them. */
	pk_more_keys();

	return key_fifo_occupied();
}

static int pk_getchar(void)
{
	while (!pk_havekey());

	return key_fifo_get();
}

static struct console_input_driver pseudo_keyboard =
{
	NULL,
	&pk_havekey,
	&pk_getchar
};

static void pk_init(void)
{
	console_add_input_driver(&pseudo_keyboard);
}

static int pk_install_on_demand_input(void)
{
	static OnDemandInput dev =
	{
		.init = &pk_init,
		.need_init = 1,
	};

	list_insert_after(&dev.list_node, &on_demand_input_devices);
	return 0;
}

INIT_FUNC(pk_install_on_demand_input);

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

#ifndef __DRIVERS_INPUT_PSEUDO_KEYBOARD_H__
#define __DRIVERS_INPUT_PSEUDO_KEYBOARD_H__

typedef enum Modifier {
	MODIFIER_NONE = 0x0,
	MODIFIER_CTRL = 0x1,
	MODIFIER_ALT = 0x2,
	MODIFIER_SHIFT = 0x4
} Modifier;

enum {
	KEY_CODE_START = 0,
	KEY_CODE_UP = 128,
	KEY_CODE_DOWN = 129,
	KEY_CODE_RIGHT = 130,
	KEY_CODE_LEFT = 131,
	KEY_CODE_END = 131,
};

/*
 * This function is expected to be implemented by every board that uses pseudo
 * keyboard driver. Input pins and the way input is interpreted can be different
 * for every board. So, common key codes are established to be passed back to
 * the pseudo driver.
 *
 * Key codes are expected as follows:
 * ASCII characters : ASCII values (0 - 127)
 * Key UP          : 128
 * Key DOWN        : 129
 * Key Right       : 130
 * Key Left        : 131
 *
 * Modifier flags are set in similar way as handled in keyboard.c. Modifiers is
 * basically a bitmask to indicate what modifiers are set.
 *
 * This function returns the number of codes read into codes array.
 */
size_t read_key_codes(Modifier *modifiers, uint16_t *codes, size_t max_codes);


#endif /* __DRIVERS_INPUT_PSEUDO_KEYBOARD_H__ */

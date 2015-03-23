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

/* Structure defining final states of pseudo keyboard state machine */
struct pk_final_state {
	int state_id;
	Modifier mod;
	uint16_t keycode;
};

/* Structure defining transition for pseudo keyboard states */
struct pk_trans {
	int src;
	int inp;
	int dst;
};

/* Structure for state machine descriptor filled in by mainboard */
struct pk_sm_desc {
	size_t total_states_count;
	int start_state;
	size_t int_states_count;
	const int *int_states_arr;
	size_t final_states_count;
	const struct pk_final_state *final_states_arr;
	size_t trans_count;
	const struct pk_trans *trans_arr;
};

/* Special input value defining no input present */
#define NO_INPUT		-1

/* Mainboard defined functions */
void mainboard_keyboard_init(struct pk_sm_desc *desc);
int mainboard_read_input(void);

#endif /* __DRIVERS_INPUT_PSEUDO_KEYBOARD_H__ */

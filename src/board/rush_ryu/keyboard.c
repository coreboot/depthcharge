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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "board/rush_ryu/state_machine.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/tegra.h"
#include "drivers/input/pseudo/keyboard.h"

/* GPIOs for reading status of buttons */
static GpioOps *pwr_btn_gpio, *vol_down_gpio, *vol_up_gpio;

/* State machine data for ryu keyboard */
struct sm_data *sm;

/* Unique state ids for all possible states in state machine */
enum {
	STATE_ID_START,
	STATE_ID_CTRL_D,
	STATE_ID_CTRL_U,
	STATE_ID_CR,
	STATE_ID_SPC,
	STATE_ID_CTRL_L,
	STATE_ID_TAB,
	STATE_ID_S1,
	STATE_NUM,
};

struct modifier_key_code {
	Modifier mod;
	uint16_t keycode;
};

/* Map state ids to key/modifier codes */
static const struct modifier_key_code state_id_to_code[] = {
	[STATE_ID_CTRL_D] = {MODIFIER_CTRL, 'D'},
	[STATE_ID_CTRL_U] = {MODIFIER_CTRL, 'U'},
	[STATE_ID_CR]     = {MODIFIER_NONE, '\r'},
	[STATE_ID_SPC]    = {MODIFIER_NONE, ' '},
	[STATE_ID_CTRL_L] = {MODIFIER_CTRL, 'L'},
	[STATE_ID_TAB]    = {MODIFIER_NONE, '\t'},
};

/* Intermediate stages */
static const int int_states[] = {
	STATE_ID_S1,
};

/* Final stages that map to key/modifier codes */
static const int final_states[] = {
	STATE_ID_CTRL_D,
	STATE_ID_CTRL_U,
	STATE_ID_CR,
	STATE_ID_SPC,
	STATE_ID_CTRL_L,
	STATE_ID_TAB,
};

struct trans {
	int src;
	int inp;
	int dst;
};

/*
 * Input is formatted as follows:
 * Bit 0 - Vol down
 * Bit 1 - Vol Up
 * Bit 2 - Pwr btn
 *
 * Pwr btn is active high, whereas vol up and down are active low, thus we need
 * to mask the input we read for active low buttons to ensure we interpret them
 * right.
 *
 */
#define KEYSET(pwr, vup, vdn)	  ((pwr) | (vup) | (vdn))

enum {
	VOL_DOWN_SHIFT,
	VOL_UP_SHIFT,
	PWR_BTN_SHIFT,
	VOL_DOWN = (1 << VOL_DOWN_SHIFT),
	VOL_UP = (1 << VOL_UP_SHIFT),
	PWR_BTN = (1 << PWR_BTN_SHIFT),
	NO_BTN_PRESSED = KEYSET(0,0,0),
};

/*
 * Key definitions are as follows:
 * Buttons:
 * Pwr = Power button
 * Vup = Volume Up button
 * Vdn = Volume Down button
 *
 * Ctrl - D = Pwr + Vup
 * Ctrl - U = Pwr + Vdn
 * CR       = Vup
 * Space    = Vdn
 * Ctrl - L = Pwr -> Vup
 * Tab      = Pwr -> Vdn
 */
static const struct trans arr[] = {
	{STATE_ID_START, KEYSET(PWR_BTN, VOL_UP, 0), STATE_ID_CTRL_D},
	{STATE_ID_START, KEYSET(PWR_BTN, 0, VOL_DOWN), STATE_ID_CTRL_U},
	{STATE_ID_START, KEYSET(0, VOL_UP, 0), STATE_ID_CR},
	{STATE_ID_START, KEYSET(0, 0, VOL_DOWN), STATE_ID_SPC},
	{STATE_ID_START, KEYSET(PWR_BTN, 0, 0), STATE_ID_S1},
	{STATE_ID_S1, KEYSET(0, VOL_UP, 0), STATE_ID_CTRL_L},
	{STATE_ID_S1, KEYSET(0, 0, VOL_DOWN), STATE_ID_TAB},
};

static void ryu_state_machine_setup(void)
{
	int i;

	/* Initialize state machine for ryu keyboard */
	sm = sm_init(STATE_NUM);

	/* Add start state */
	sm_add_start_state(sm, STATE_ID_START);

	/* Add intermediate states */
	for (i = 0; i < ARRAY_SIZE(int_states); i++)
		sm_add_nonfinal_state(sm, int_states[i]);

	/* Add final states */
	for (i = 0; i < ARRAY_SIZE(final_states); i++)
		sm_add_final_state(sm, final_states[i]);

	/* Add valid transitions */
	for (i = 0; i < ARRAY_SIZE(arr); i++)
		sm_add_transition(sm, arr[i].src, arr[i].inp, arr[i].dst);
}

static int ryu_keyboard_init(void)
{
	pwr_btn_gpio = sysinfo_lookup_gpio("power", 1,
					   new_tegra_gpio_input_from_coreboot);
	die_if(!pwr_btn_gpio, "No GPIO for power!!\n");

	/* Inputs volup and voldown are active low. */
	vol_down_gpio = new_gpio_not(&new_tegra_gpio_input(GPIO(Q, 6))->ops);
	vol_up_gpio = new_gpio_not(&new_tegra_gpio_input(GPIO(Q, 7))->ops);

	ryu_state_machine_setup();
	return 0;
}
INIT_FUNC(ryu_keyboard_init);

static uint8_t read_input(void)
{
	uint8_t input;

	input = (pwr_btn_gpio->get(pwr_btn_gpio) << PWR_BTN_SHIFT) |
		(vol_up_gpio->get(vol_up_gpio) << VOL_UP_SHIFT) |
		(vol_down_gpio->get(vol_down_gpio) << VOL_DOWN_SHIFT);

	return input;
}

size_t read_key_codes(Modifier *modifiers, uint16_t *codes, size_t max_codes)
{
	assert (modifiers && codes && max_codes);

	int i = 0;

	sm_reset_state(sm);

	/* We have only max_codes space in codes to fill up key codes. */
	while (i < max_codes) {

		uint8_t input;
		uint64_t start = timer_us(0);
		/* If no button is pressed for 500 msec, return. */
		uint64_t timeout_us = 500 * 1000;
		int ret;
		int output;

		do {
			uint64_t button_press = timer_us(0);
			uint64_t button_timeout = 100 * 1000;
			input = read_input();

			if (input == NO_BTN_PRESSED)
				continue;

			/* Input should be seen for at least 100 ms */
			do {
				if (read_input() != input) {
					input = NO_BTN_PRESSED;
					break;
				}
			} while (timer_us(button_press) < button_timeout);

			/*
			 * If button is pressed, wait until input changes to
			 * avoid duplicate entries for same input.
			 */
			if (input != NO_BTN_PRESSED) {
				while (read_input() == input)
					;
				break;
			}
		} while (timer_us(start) < timeout_us);

		/* If timeout without input, return. */
		if (input == NO_BTN_PRESSED)
			break;

		/* Run state machine to move to next state */
		ret = sm_run(sm, input, &output);

		if (ret == STATE_NOT_FINAL)
			continue;

		if (ret == STATE_NO_TRANSITION) {
			sm_reset_state(sm);
			continue;
		}

		assert(output < STATE_NUM);
		const struct modifier_key_code *ptr = &state_id_to_code[output];
		*modifiers |= ptr->mod;
		codes[i++] = ptr->keycode;
	}

	return i;
}

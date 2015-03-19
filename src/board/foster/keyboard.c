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

#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/tegra210.h"
#include "drivers/input/pseudo/keyboard.h"

/* GPIOs for reading status of buttons */
static GpioOps *pwr_btn_gpio, *vol_down_gpio, *vol_up_gpio;

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

/* Intermediate stages */
static const int int_states[] = {
	STATE_ID_S1,
};

/* Final stages that map to key/modifier codes */
static const struct pk_final_state final_states[] = {
	{STATE_ID_CTRL_D, MODIFIER_CTRL, 'D'},
	{STATE_ID_CTRL_U, MODIFIER_CTRL, 'U'},
	{STATE_ID_CR, MODIFIER_NONE, '\r'},
	{STATE_ID_SPC, MODIFIER_NONE, ' '},
	{STATE_ID_CTRL_L, MODIFIER_CTRL, 'L'},
	{STATE_ID_TAB, MODIFIER_NONE, '\t'},
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
static const struct pk_trans trans_arr[] = {
	{STATE_ID_START, KEYSET(PWR_BTN, VOL_UP, 0), STATE_ID_CTRL_D},
	{STATE_ID_START, KEYSET(PWR_BTN, 0, VOL_DOWN), STATE_ID_CTRL_U},
	{STATE_ID_START, KEYSET(0, VOL_UP, 0), STATE_ID_CR},
	{STATE_ID_START, KEYSET(0, 0, VOL_DOWN), STATE_ID_SPC},
	{STATE_ID_START, KEYSET(PWR_BTN, 0, 0), STATE_ID_S1},
	{STATE_ID_S1, KEYSET(0, VOL_UP, 0), STATE_ID_CTRL_L},
	{STATE_ID_S1, KEYSET(0, 0, VOL_DOWN), STATE_ID_TAB},
};

static void foster_sm_init(struct pk_sm_desc *desc)
{
	desc->total_states_count = STATE_NUM;
	desc->start_state = STATE_ID_START;
	desc->int_states_count = ARRAY_SIZE(int_states);
	desc->int_states_arr = int_states;
	desc->final_states_count = ARRAY_SIZE(final_states);
	desc->final_states_arr = final_states;
	desc->trans_count = ARRAY_SIZE(trans_arr);
	desc->trans_arr = trans_arr;
}

void mainboard_keyboard_init(struct pk_sm_desc *desc)
{
	pwr_btn_gpio = sysinfo_lookup_gpio("power", 1,
					   new_tegra_gpio_input_from_coreboot);
	die_if(!pwr_btn_gpio, "No GPIO for power!!\n");

	/* Inputs volup and voldown are active low. */
	vol_down_gpio = new_gpio_not(&new_tegra_gpio_input(GPIO(X, 7))->ops);
	vol_up_gpio = new_gpio_not(&new_tegra_gpio_input(GPIO(X, 6))->ops);

	foster_sm_init(desc);
}

int mainboard_read_input(void)
{
	int input;

	input = (pwr_btn_gpio->get(pwr_btn_gpio) << PWR_BTN_SHIFT) |
		(vol_up_gpio->get(vol_up_gpio) << VOL_UP_SHIFT) |
		(vol_down_gpio->get(vol_down_gpio) << VOL_DOWN_SHIFT);

	if (input == NO_BTN_PRESSED)
		input = -1;

	return input;
}

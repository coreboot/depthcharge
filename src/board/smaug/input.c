/*
 * Copyright 2015 Google Inc.
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

#include "board/smaug/input.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/tegra210.h"

/* GPIOs for reading status of buttons */
static GpioOps *pwr_btn_gpio, *vol_down_gpio, *vol_up_gpio;

static void button_gpio_init(void)
{
	static int initialized = 0;

	if (initialized)
		return;

	/* Inputs powerbtn, volup and voldown are active low. */
	pwr_btn_gpio = new_gpio_not(&new_tegra_gpio_input(GPIO(X, 5))->ops);
	vol_down_gpio = new_gpio_not(&new_tegra_gpio_input(GPIO(X, 7))->ops);
	vol_up_gpio = new_gpio_not(&new_tegra_gpio_input(GPIO(M, 4))->ops);

	initialized = 1;
}

static uint8_t mainboard_read_input(void)
{
	uint8_t input;

	button_gpio_init();

	input = (pwr_btn_gpio->get(pwr_btn_gpio) << PWR_BTN_SHIFT) |
		(vol_up_gpio->get(vol_up_gpio) << VOL_UP_SHIFT) |
		(vol_down_gpio->get(vol_down_gpio) << VOL_DOWN_SHIFT);

	return input;
}

uint8_t read_char(uint64_t timeout_us)
{
	/* Button should be pressed for at least 100 ms */
	uint64_t button_timeout_us = 100 * 1000;
	uint64_t button_press_us;
	uint8_t input;
	uint64_t input_us = timer_us(0);

	/* If no user input in 1 minute, return NO_BTN_PRESSED. */
	while (timer_us(input_us) < timeout_us) {
		input = mainboard_read_input();

		if (input == NO_BTN_PRESSED)
			continue;

		button_press_us = timer_us(0);

		do {
			if (mainboard_read_input() != input) {
				input = NO_BTN_PRESSED;
				break;
			}
		} while (timer_us(button_press_us) < button_timeout_us);

		if (input != NO_BTN_PRESSED) {
			while (mainboard_read_input() == input)
				;
			break;
		}

		/* Reset user input timeout. */
		input_us = timer_us(0);
	}

	return input;
}

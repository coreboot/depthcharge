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

#include <libpayload.h>

/*
 * Input is formatted as follows:
 * Bit 0 - Vol down
 * Bit 1 - Vol Up
 * Bit 2 - Pwr btn
 *
 * Pwr btn, vol up and down are active low, thus we need to mask the input we
 * read for active low buttons to ensure we interpret them right.
 *
 */
#define KEYSET(pwr, vup, vdn)  ((pwr) | (vup) | (vdn))

enum {
	VOL_DOWN_SHIFT,
	VOL_UP_SHIFT,
	PWR_BTN_SHIFT,
	VOL_DOWN = (1 << VOL_DOWN_SHIFT),
	VOL_UP = (1 << VOL_UP_SHIFT),
	PWR_BTN = (1 << PWR_BTN_SHIFT),
	NO_BTN_PRESSED = KEYSET(0,0,0),
};

uint8_t read_char(uint64_t timeout_us);

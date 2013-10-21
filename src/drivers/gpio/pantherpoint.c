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
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <stdint.h>

#include "drivers/gpio/pantherpoint.h"

static uint8_t pantherpoint_use_start[PANTHERPOINT_NUM_GPIO_BANKS] =
	{ 0x00, 0x30, 0x40 };
static uint8_t pantherpoint_io_start[PANTHERPOINT_NUM_GPIO_BANKS] =
	{ 0x04, 0x34, 0x44 };
static uint8_t pantherpoint_lvl_start[PANTHERPOINT_NUM_GPIO_BANKS] =
	{ 0x0c, 0x38, 0x48 };

static PchGpioCfg pantherpoint_gpio_cfg = {
	.num_banks = PANTHERPOINT_NUM_GPIO_BANKS,
	.use_start = pantherpoint_use_start,
	.io_start =  pantherpoint_io_start,
	.lvl_start = pantherpoint_lvl_start,
};

PchGpio *new_pantherpoint_gpio_input(unsigned bank, unsigned bit)
{
	return new_pch_gpio_input(&pantherpoint_gpio_cfg, bank, bit);
}

PchGpio *new_pantherpoint_gpio_output(unsigned bank, unsigned bit)
{
	return new_pch_gpio_output(&pantherpoint_gpio_cfg, bank, bit);
}

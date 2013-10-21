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

#include "drivers/gpio/baytrail.h"

/* 6 banks: SC[31:0] SC[63:32] SC[95:64] SC[101:96] SSUS[31:0] SSUS[43:32] */
static uint8_t baytrail_use_start[BAYTRAIL_NUM_GPIO_BANKS] =
	{ 0x00, 0x20, 0x40, 0x60, 0x80, 0xA0 };
static uint8_t baytrail_io_start[BAYTRAIL_NUM_GPIO_BANKS] =
	{ 0x04, 0x24, 0x44, 0x64, 0x84, 0xA4 };
static uint8_t baytrail_lvl_start[BAYTRAIL_NUM_GPIO_BANKS] =
	{ 0x08, 0x28, 0x48, 0x68, 0x88, 0xA8 };

static PchGpioCfg baytrail_gpio_cfg = {
	.num_banks = BAYTRAIL_NUM_GPIO_BANKS,
	.use_start = baytrail_use_start,
	.io_start =  baytrail_io_start,
	.lvl_start = baytrail_lvl_start,
};

PchGpio *new_baytrail_gpio_input(unsigned bank, unsigned bit)
{
	return new_pch_gpio_input(&baytrail_gpio_cfg, bank, bit);
}

PchGpio *new_baytrail_gpio_output(unsigned bank, unsigned bit)
{
	return new_pch_gpio_output(&baytrail_gpio_cfg, bank, bit);
}

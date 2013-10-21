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

#ifndef __DRIVERS_GPIO_BAYTRAIL_H__
#define __DRIVERS_GPIO_BAYTRAIL_H__

#include "drivers/gpio/pch.h"

/* 6 banks: SC[31:0] SC[63:32] SC[95:64] SC[101:96] SSUS[31:0] SSUS[43:32] */
enum {
	BAYTRAIL_BANK_SC_31_0 = 0,
	BAYTRAIL_BANK_SC_64_32,
	BAYTRAIL_BANK_SC_95_64,
	BAYTRAIL_BANK_SC_101_96,
	BAYTRAIL_BANK_SSUS_31_0,
	BAYTRAIL_BANK_SSUS_43_32,
	BAYTRAIL_NUM_GPIO_BANKS
};

PchGpio *new_baytrail_gpio_input(unsigned bank, unsigned bit);
PchGpio *new_baytrail_gpio_output(unsigned bank, unsigned bit);

#endif /* __DRIVERS_GPIO_BAYTRAIL_H__ */

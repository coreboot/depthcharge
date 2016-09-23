/*
 * Copyright (C) 2016 Marvell, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdint.h>
#include <libpayload.h>
#include "vboot/util/flag.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/mvmap2315.h"

/*
 * We are using hardcoded placeholders until we have more
 * information about the hardware pins these will be
 * connected to
 */

static int board_ld(struct GpioOps *me)
{
	return 1;
}

GpioOps board_lid = {
	.get = &board_ld
};

static int board_pwr(struct GpioOps *me)
{
	return 0;
}

GpioOps board_power = {
	.get = &board_pwr
};

void mvmap2315_gpio_setup(void)
{
	flag_install(FLAG_LIDSW, &board_lid);
	flag_install(FLAG_PWRSW, &board_power);
}


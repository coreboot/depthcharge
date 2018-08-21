/*
 * Copyright 2018 Google Inc.
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
 */

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "drivers/bus/spi/mtk.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/mt8183.h"
#include "drivers/gpio/sysinfo.h"

static int board_setup(void)
{
	sysinfo_install_flags(new_mtk_gpio_input);

	MtkSpi *spi1 = new_mtk_spi(0x11010000);
	flash_set_ops(&new_spi_flash(&spi1->ops)->ops);

	return 0;
}

INIT_FUNC(board_setup);

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

#include "base/init_funcs.h"
#include "boot/fit.h"
#include "drivers/flash/memmapped.h"
#include "drivers/flash/spi.h"

static int board_setup(void)
{
	fit_set_compat("marvell,mvmap2315");

	MemMappedFlash *bSpiFlash = new_mem_mapped_flash(0x400000, 0x400000);

	flash_set_ops((FlashOps *)bSpiFlash);

	return 0;
}

INIT_FUNC(board_setup);

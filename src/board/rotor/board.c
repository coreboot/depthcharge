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
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/mvmap2315.h"
#include "drivers/storage/mvmap2315_mmc.h"
#include "vboot/util/flag.h"

static int board_setup(void)
{
	Mvmap2315MmcHost *sd_card;

	fit_add_compat("marvell,mvmap2315");

	MemMappedFlash *bSpiFlash = new_mem_mapped_flash(0x400000, 0x400000);

	flash_set_ops((FlashOps *)bSpiFlash);

	sd_card = new_mvmap2315_mmc_host(0xE0138800,
					 4,
					 400000,
					 50000000,
					 200000000);

	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);
	mvmap2315_gpio_setup();

	return 0;
}

INIT_FUNC(board_setup);

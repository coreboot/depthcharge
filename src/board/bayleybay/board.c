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

#include <libpayload.h>

#include "base/init_funcs.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/power/pch.h"
#include "drivers/storage/sdhci.h"

static int board_setup(void)
{
	flash_set_ops(&new_mem_mapped_flash(0xff800000, 0x800000)->ops);

	power_set_ops(&pch_power_ops);

	/*
	 * Initting port zero (eMMC) only for now. Device IDs other two ports
	 * are 0x0F15 and 0x0F16. Clock frequencies for the eMMC port are 400
	 * KHz min (used during initialization) and 52 MHz max (firmware does
	 * not use 200 MHz aka HS mode).
	 */
	SdhciHost *emmc = new_pci_sdhci_host(PCI_DEV(0, 23, 0), 0,
					     400 * 1000, 200 * 1000 * 1000);

	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	return 0;
}

INIT_FUNC(board_setup);

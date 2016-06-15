/*
 * Copyright (C) 2016 Intel Corporation.
 * Copyright 2016 Google Inc.
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
 */

#include <libpayload.h>
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/tpm/lpc.h"
#include "drivers/tpm/tpm.h"
#include "drivers/power/pch.h"
#include "drivers/storage/sdhci.h"

#define EMMC_SD_CLOCK_MIN       400000
#define EMMC_CLOCK_MAX          200000000

/*
 * FIXME: this needs to be derived differently. The coreboot code actually
 * handles the situation better than assuming a specific descriptor layout
 * and re-calculating the same information. Nevertheless in order to
 * utilize the exist set of flash ops one needs to assume the following layout:
 *
 *         0 +----------------------+
 *           | descriptor           |
 *        4K +----------------------+
 *           | BIOS region          |
 *         X +----------------------+
 *           | CSE device extension |
 * rom size  +----------------------+
 *
 * The BIOS region is the only thing memory-mapped. And one needs to include
 * the size of the descriptor in order for the math to work out within the
 * memory-mapped flash implementation. In short, the 'X' above is the size
 * used for FLASH_MEM_MAP_SIZE.
 *
 */
#define FLASH_MEM_MAP_SIZE 0xf7f000
#define FLASH_MEM_MAP_BASE ((uintptr_t)(0x100000000ULL - FLASH_MEM_MAP_SIZE))

static int board_setup(void)
{
	sysinfo_install_flags(NULL);

	/* EC */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, 0, NULL);
	register_vboot_ec(&cros_ec->vboot, 0);

	/* W25Q128FV SPI Flash */
	flash_set_ops(&new_mem_mapped_flash(FLASH_MEM_MAP_BASE,
					 FLASH_MEM_MAP_SIZE)->ops);

	/* FIXME: not stuffed but need MOCK_TPM to work. */
	tpm_set_ops(&new_lpc_tpm((void *)(uintptr_t)0xfed40000)->ops);

	SdhciHost *emmc;
	emmc = new_pci_sdhci_host(PCI_DEV(0, 0x1c, 0), 0,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* PCH Power */
	power_set_ops(&apollolake_power_ops);

	return 0;
}

INIT_FUNC(board_setup);

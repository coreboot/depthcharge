/*
 * Copyright (C) 2018 Intel Corporation
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

/*
 * These needs to be included first.
 * Some of the driver headers would be dependent on these.
 */
#include <pci.h>
#include <pci/pci.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/icelake.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/icelake.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/ahci.h"
#include <libpayload.h>
#include <sysinfo.h>

/*
 * Clock frequencies for the eMMC and SD ports are defined below. The minimum
 * frequency is the same for both interfaces, the firmware does not run any
 * interface faster than 52 MHz, but defines maximum eMMC frequency as 200 MHz
 * for proper divider settings.
 */
#define EMMC_SD_CLOCK_MIN	400000
#define EMMC_CLOCK_MAX		200000000
#define SD_CLOCK_MAX		52000000

static int board_setup(void)
{
	sysinfo_install_flags(new_icelake_gpio_input_from_coreboot);

	/* 16MB SPI Flash */
	flash_set_ops(&new_mem_mapped_flash(0xff000000, 0x1000000)->ops);

	/* PCH Power */
	power_set_ops(&icelake_power_ops);

	/* SATA */
	AhciCtrlr *ahci = new_ahci_ctrlr(PCH_DEV_SATA);
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	/* eMMC */
	SdhciHost *emmc = new_pci_sdhci_host(PCH_DEV_EMMC),
			SDHCI_PLATFORM_SUPPORTS_HS400ES,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* SD Card */
	SdhciHost *sd = new_pci_sdhci_host(PCH_DEV_SDCARD, 1,
			EMMC_SD_CLOCK_MIN, SD_CLOCK_MAX);
	list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
			&removable_block_dev_controllers);

	/* NVMe */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCH_DEV_PCIE9);
	list_insert_after(&nvme->ctrlr.list_node,
			  &fixed_block_dev_controllers);

	return 0;
}

INIT_FUNC(board_setup);

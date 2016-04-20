/*
 * Copyright (C) 2016 Intel Corporation.
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
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/flash/memmapped.h"
#include "drivers/tpm/tpm.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/power/pch.h"
#include "drivers/tpm/lpc.h"
#include "drivers/storage/sdhci.h"

#define EMMC_SD_CLOCK_MIN	400000
#define SD_CLOCK_MAX		52000000
#define EMMC_CLOCK_MAX		200000000

/* Flash memory map size includes the 4K descriptor which is not accessible */
#define FLASH_MEM_MAP_SIZE      0x77F000
#define FLASH_MEM_MAP_BASE      ((uintptr_t)(0x100000000ULL - FLASH_MEM_MAP_SIZE))


static int board_setup(void)
{
	sysinfo_install_flags(NULL);

	/* W25Q128FV SPI Flash */
	flash_set_ops(&new_mem_mapped_flash(FLASH_MEM_MAP_BASE, FLASH_MEM_MAP_SIZE)->ops);

	/* SLB9670 SPI TPM */
	tpm_set_ops(&new_lpc_tpm((void *)(uintptr_t)0xfed40000)->ops);

	SdhciHost *emmc;
	emmc = new_pci_sdhci_host(PCI_DEV(0, 0x1c, 0), 0,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* SD Card (if present) */
	pcidev_t sd_pci_dev = PCI_DEV(0, 0x1b, 0);
	uint16_t sd_vendor_id = pci_read_config32(sd_pci_dev, REG_VENDOR_ID);
	if (sd_vendor_id == PCI_VENDOR_ID_INTEL) {
		SdhciHost *sd = new_pci_sdhci_host(sd_pci_dev, 1,
					EMMC_SD_CLOCK_MIN, SD_CLOCK_MAX);
		list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
					&removable_block_dev_controllers);
	}

	/* PCH Power */
	power_set_ops(&apollolake_power_ops);

	return 0;
}

INIT_FUNC(board_setup);

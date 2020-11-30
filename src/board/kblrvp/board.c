/*
 * Copyright (C) 2015 Google Inc.
 * Copyright (C) 2017 Intel Corporation
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

/*
 * These needs to be included first.
 * Some of the driver headers would be dependent on these.
 */
#include <pci.h>
#include <pci/pci.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/lpc.h"
#include "drivers/tpm/tpm.h"
#include <libpayload.h>
#include <sysinfo.h>
#include "vboot/util/flag.h"
#include "vboot/util/commonparams.h"

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
	sysinfo_install_flags(NULL);

	/* SPI TPM memory mapped to act like LPC TPM */
	tpm_set_ops(&new_lpc_tpm((void *)(uintptr_t)0xfed40000)->ops);

	/* PCH Power */
	power_set_ops(&skylake_power_ops);

	/* SATA SSD */
	AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV(0, 0x17, 0));
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	/* SATA controller on bus 1 dev/fun 0 */
	AhciCtrlr *ahci1 = new_ahci_ctrlr(PCI_DEV(1, 0x00, 0));
	list_insert_after(&ahci1->ctrlr.list_node, &fixed_block_dev_controllers);

        /* PCIe NVME */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(0, 0x1c, 4));
	list_insert_after(&nvme->ctrlr.list_node, &fixed_block_dev_controllers);

	/* eMMC */
	SdhciHost *emmc = new_pci_sdhci_host(PCI_DEV(0, 0x1e, 4),
			SDHCI_PLATFORM_NO_EMMC_HS200,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* SD Card */
	SdhciHost *sd = new_pci_sdhci_host(PCI_DEV(0, 0x1e, 6), 1,
			EMMC_SD_CLOCK_MIN, SD_CLOCK_MAX);

	list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
			&removable_block_dev_controllers);

	return 0;
}

INIT_FUNC(board_setup);

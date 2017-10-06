/*
 * Copyright (C) 2015 Google Inc.
 * Copyright (C) 2016 Intel Corporation
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
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/storage/ahci.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/lpc.h"
#include "drivers/tpm/tpm.h"
#include <gbb_header.h>
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
#define EMMC_CLOCK_MAX		25000000
#define SD_CLOCK_MAX		52000000

static int board_setup(void)
{
	uint8_t secondary_bus;

	sysinfo_install_flags(NULL);

	/* 16MB SPI Flash */
	flash_set_ops(&new_mem_mapped_flash(0xff000000, 0x1000000)->ops);

	/* PCH Power */
	power_set_ops(&cannonlake_power_ops);

	uintptr_t UsbMmioBase =
		pci_read_config32(PCI_DEV(0, 0x14, 0), PCI_BASE_ADDRESS_0);
	UsbMmioBase &= 0xFFFF0000; /* 32 bits only */
	UsbHostController *usb_host1 = new_usb_hc(XHCI, UsbMmioBase);
	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

        /* SATA SSD */
        AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV(0, 0x17, 0));
        list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	/* eMMC */
	SdhciHost *emmc = new_pci_sdhci_host(PCI_DEV(0, 0x1a, 0), 0,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* NVME SSD */
	secondary_bus = pci_read_config8(PCI_DEV(0, 0x1D, 0),
			REG_SECONDARY_BUS);
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(secondary_bus, 0, 0));
	list_insert_after(&nvme->ctrlr.list_node, &fixed_block_dev_controllers);

	return 0;
}

INIT_FUNC(board_setup);

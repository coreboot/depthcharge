// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2022 Intel Corporation */

/*
 * These needs to be included first.
 * Some of the driver headers would be dependent on these.
 */
#include <commonlib/list.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/init_funcs.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/power/pch.h"
#include "drivers/soc/meteorlake.h"
#include "drivers/storage/ahci.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/ufs.h"
#include "drivers/storage/ufs_intel.h"
#include <libpayload.h>
#include <sysinfo.h>
#include <libpayload.h>
#include <sysinfo.h>

static int board_setup(void)
{
	uint8_t secondary_bus;

	sysinfo_install_flags(NULL);

	/* Chrome EC (eSPI) */
	if (CONFIG(DRIVER_EC_CROS_LPC)) {
		CrosEcLpcBus *cros_ec_lpc_bus;
		if (CONFIG(CROS_EC_ENABLE_MEC)) {
			cros_ec_lpc_bus =
				new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_MEC);
		}
		else {
			cros_ec_lpc_bus =
				new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
		}

		CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
		register_vboot_ec(&cros_ec->vboot);
	}

	/* Power */
	power_set_ops(&meteorlake_power_ops);

	/* Flash */
	flash_set_ops(&new_mmap_flash()->ops);

	/* RP1*/
	secondary_bus = pci_read_config8(PCI_DEV_PCIE1, REG_SECONDARY_BUS);
	NvmeCtrlr *nvme_1 = new_nvme_ctrlr(PCI_DEV(secondary_bus, 0, 0));
	list_insert_after(&nvme_1->ctrlr.list_node, &fixed_block_dev_controllers);

	/* RP9*/
	secondary_bus = pci_read_config8(PCI_DEV_PCIE9, REG_SECONDARY_BUS);
	NvmeCtrlr *nvme_2 = new_nvme_ctrlr(PCI_DEV(secondary_bus, 0, 0));
	list_insert_after(&nvme_2->ctrlr.list_node, &fixed_block_dev_controllers);

	/* RP10 */
	secondary_bus = pci_read_config8(PCI_DEV_PCIE10, REG_SECONDARY_BUS);
	NvmeCtrlr *nvme_3 = new_nvme_ctrlr(PCI_DEV(secondary_bus, 0, 0));
	list_insert_after(&nvme_3->ctrlr.list_node, &fixed_block_dev_controllers);

	/* RP11 */
	secondary_bus = pci_read_config8(PCI_DEV_PCIE11, REG_SECONDARY_BUS);
	NvmeCtrlr *nvme_4 = new_nvme_ctrlr(PCI_DEV(secondary_bus, 0, 0));
	list_insert_after(&nvme_4->ctrlr.list_node, &fixed_block_dev_controllers);

	/* SATA */
	AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV_SATA);
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	/* UFS */
	if (CONFIG(DRIVER_STORAGE_UFS_INTEL)) {
		IntelUfsCtlr *intel_ufs = new_intel_ufs_ctlr(PCI_DEV_UFS,
				UFS_REFCLKFREQ_38_4);
		list_insert_after(&intel_ufs->ufs.bctlr.list_node,
				&fixed_block_dev_controllers);
	}

	return 0;
}

INIT_FUNC(board_setup);

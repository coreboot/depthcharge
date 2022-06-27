// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2022 Intel Corporation */

/*
 * These needs to be included first.
 * Some of the driver headers would be dependent on these.
 */
#include <pci.h>
#include <pci/pci.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/power/pch.h"
#include "drivers/soc/meteorlake.h"
#include "drivers/storage/ahci.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include <libpayload.h>
#include <sysinfo.h>
#include <libpayload.h>
#include <sysinfo.h>

static int board_setup(void)
{
	uint8_t secondary_bus;

	sysinfo_install_flags(NULL);

	/* Power */
	power_set_ops(&meteorlake_power_ops);

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

	return 0;
}

INIT_FUNC(board_setup);
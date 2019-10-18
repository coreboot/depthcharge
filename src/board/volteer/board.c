// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2019 Google Inc.  */

/*
 * These needs to be included first.
 * Some of the driver headers would be dependent on these.
 */
#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"


static int board_setup(void)
{
	sysinfo_install_flags(NULL);

	/* Chrome EC (eSPI) */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, 0, NULL);
	register_vboot_ec(&cros_ec->vboot, 0);

	/* 32MB SPI Flash */
	flash_set_ops(&new_mem_mapped_flash(0xfe000000, 0x2000000)->ops);

	/* PCH Power */
	power_set_ops(&cannonlake_power_ops);

	/* NVME SSD */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(0, 0x1d, 0));
	list_insert_after(&nvme->ctrlr.list_node, &fixed_block_dev_controllers);

	return 0;
}

INIT_FUNC(board_setup);

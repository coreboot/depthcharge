// SPDX-License-Identifier: GPL-2.0-only
/*
 * Intel UFS Driver
 *
 * Copyright (C) 2022, Intel Corporation.
 */

#include <pci.h>
#include <stdlib.h>

#include "base/container_of.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/ufs.h"
#include "drivers/storage/ufs_intel.h"

static int intel_ufs_hook_fn(UfsCtlr *ufs, UfsHookOp op, void *data)
{
	switch (op) {
	case UFS_OP_PRE_LINK_STARTUP:
		return ufs_dme_set(ufs, PA_LOCAL_TX_LCC_ENABLE, 0);
	default:
		break;
	};
	return 0;
}

static int intel_ufs_update(BlockDevCtrlrOps *bdev_ops)
{
	IntelUfsCtlr *intel_ufs = container_of(bdev_ops, IntelUfsCtlr,
					       ufs.bctlr.ops);
	pcidev_t dev = intel_ufs->dev;

	if (!intel_ufs->ufs.hci_base) {
		intel_ufs->ufs.hci_base = (void *)(pci_read_config32(dev, REG_BAR0) & ~0xf);
		intel_ufs->ufs.hook_fn  = intel_ufs_hook_fn;
		pci_set_bus_master(dev);
	}

	return ufs_update(bdev_ops);
}

IntelUfsCtlr *new_intel_ufs_ctlr(pcidev_t dev)
{
	IntelUfsCtlr *intel_ufs = xzalloc(sizeof(IntelUfsCtlr));

	printf("Looking for UFS Controller %02x:%02x:%02x\n",
			PCI_BUS(dev),PCI_SLOT(dev),PCI_FUNC(dev));

	intel_ufs->ufs.bctlr.ops.update = intel_ufs_update;
	intel_ufs->ufs.bctlr.need_update = 1;
	intel_ufs->dev = dev;

	return intel_ufs;
}

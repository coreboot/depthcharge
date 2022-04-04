// SPDX-License-Identifier: GPL-2.0-only
/*
 * Intel UFS Driver
 *
 * Copyright (C) 2022, Intel Corporation.
 */

#ifndef __DRIVERS_STORAGE_UFS_INTEL_H__
#define __DRIVERS_STORAGE_UFS_INTEL_H__

#include <pci.h>

#include "drivers/storage/ufs.h"

typedef struct IntelUfsCtlr {
	UfsCtlr ufs;
	pcidev_t dev;
} IntelUfsCtlr;

IntelUfsCtlr *new_intel_ufs_ctlr(pcidev_t dev);

#endif // __DRIVERS_STORAGE_UFS_INTEL_H__

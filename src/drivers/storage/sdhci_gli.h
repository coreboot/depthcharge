/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2020 Genesys Logic Inc. */

#ifndef __DRIVER_STORAGE_SDHCI_GLI_H__
#define __DRIVER_STORAGE_SDHCI_GLI_H__

#include "drivers/storage/sdhci.h"

SdhciHost *new_gl9763e_sdhci_host(pcidev_t dev, unsigned int platform_info,
				  int clock_min, int clock_max);
#endif

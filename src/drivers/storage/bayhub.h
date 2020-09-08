/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2019 Google LLC. */

#ifndef __DRIVER_STORAGE_BAYHUB_H__
#define __DRIVER_STORAGE_BAYHUB_H__

#include "sdhci.h"

SdhciHost *new_bayhub_sdhci_host(pcidev_t dev, unsigned int platform_info,
				 int clock_min, int clock_max);

#endif

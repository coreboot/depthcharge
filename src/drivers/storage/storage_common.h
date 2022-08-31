/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright 2022 Google LLC. */

#ifndef __DRIVERS_STORAGE_SETUP_STORAGE_H__
#define __DRIVERS_STORAGE_SETUP_STORAGE_H__

#include "base/fw_config.h"

enum storage_media {
	STORAGE_NVME,
	STORAGE_SDHCI,
	STORAGE_EMMC,
	STORAGE_RTKMMC,
	STORAGE_UFS,
};

struct emmc_config {
	unsigned int platform_flags;
	unsigned int clock_min;
	unsigned int clock_max;
};

struct storage_config {
	enum storage_media media;
	pcidev_t pci_dev;
	struct emmc_config emmc;
	const struct fw_config *fw_config;
};

#endif /* __DRIVERS_STORAGE_SETUP_STORAGE_H__ */

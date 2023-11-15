/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright 2022 Google LLC. */

#ifndef __DRIVERS_STORAGE_SETUP_STORAGE_H__
#define __DRIVERS_STORAGE_SETUP_STORAGE_H__

#include "base/fw_config.h"
#include "ufs.h"

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

struct ufs_config {
	UfsRefClkFreq ref_clk_freq;
};

struct storage_config {
	enum storage_media media;
	pcidev_t pci_dev;
	struct emmc_config emmc;
	struct ufs_config ufs;
	const struct fw_config *fw_config;
};

/* Returns the storage_config used by each variant.
 *
 * Declare the prototype here so Depthcharge can build.
 * However, it's not implemented in storage_common.c to force each
 * board/variant that includes this file to do it instead. This allows
 * for generating the error at build-time, rather than failing at
 * run-time if no storage configs are provided.
 */
const struct storage_config *variant_get_storage_configs(size_t *count);

#endif /* __DRIVERS_STORAGE_SETUP_STORAGE_H__ */

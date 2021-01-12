// SPDX-License-Identifier: GPL-2.0
/* Copyright 2020 Genesys Logic Inc. */

#include <libpayload.h>

#include "drivers/storage/sdhci_gli.h"

#define GL9763E_SDHC_EMMC_CTRL	0x52C
#define   ENHANCED_STROBE	(1 << 8)

static void gl9763e_set_ios(MmcCtrlr *mmc_ctrlr)
{
	u32 ctrl;
	SdhciHost *host = container_of(mmc_ctrlr, SdhciHost, mmc_ctrlr);

	sdhci_set_ios(mmc_ctrlr);

	/* configure HS400 Enhanced Store */
	ctrl = sdhci_readl(host, GL9763E_SDHC_EMMC_CTRL);
	if (mmc_ctrlr->timing == MMC_TIMING_MMC_HS400ES)
		ctrl |= ENHANCED_STROBE;
	else
		ctrl &= ~ENHANCED_STROBE;
	sdhci_writel(host, ctrl, GL9763E_SDHC_EMMC_CTRL);
}

SdhciHost *new_gl9763e_sdhci_host(pcidev_t dev, unsigned int platform_info,
				  int clock_min, int clock_max)
{
	SdhciHost *host;

	host = new_pci_sdhci_host(dev, platform_info, clock_min, clock_max);
	host->mmc_ctrlr.set_ios = &gl9763e_set_ios;

	return host;
}

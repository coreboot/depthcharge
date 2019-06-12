// SPDX-License-Identifier: GPL-2.0
/* Copyright 2019 Google LLC. */

#include <libpayload.h>

#include "drivers/storage/bayhub.h"
#include "drivers/storage/sdhci.h"

#define O2_SD_VENDOR_SETTING	0x110
#define O2_SD_HW_TUNING_DISABLE	(1<<4)

static int bh720_execute_tuning(struct MmcMedia *media)
{
	int bus_width, ret = 0;
	u16 reg;
	u32 ctrl;
	uint64_t start;

	SdhciHost *sdhc = container_of(media->ctrlr, SdhciHost, mmc_ctrlr);
	if (media->ctrlr->timing != MMC_TIMING_MMC_HS200) {
		printf("%s: Tuning only supports HS200\n", sdhc->name);
		return MMC_SUPPORT_ERR;
	}

	bus_width = media->ctrlr->bus_width;

	/* Hardware tuning only supports 4 bit */
	media->ctrlr->bus_width = 4;
	media->ctrlr->set_ios(media->ctrlr);

	/* Enable hardware tuning */
	reg = sdhci_readw(sdhc, O2_SD_VENDOR_SETTING);
	reg &= ~O2_SD_HW_TUNING_DISABLE;
	sdhci_writew(sdhc, reg, O2_SD_VENDOR_SETTING);

	/* Start tuning */
	reg = sdhci_readw(sdhc, SDHCI_HOST_CONTROL2);
	reg |= SDHCI_CTRL_EXEC_TUNING;
	sdhci_writew(sdhc, reg, SDHCI_HOST_CONTROL2);

	ret = sdhci_send_hs200_tuning_cmd(media->ctrlr);
	if (ret) {
		printf("%s: Failed to send tuning command\n", sdhc->name);
		goto tuning_failed;
	}

	/* Wait for tuning to finish */
	start = timer_us(0);
	while (1) {
		ctrl = sdhci_readl(sdhc, SDHCI_HOST_CONTROL2);
		if (!(ctrl & SDHCI_CTRL_EXEC_TUNING))
			break;

		if (timer_us(start) > SDHCI_TUNING_MAX_US) {
			printf("%s: Tuning timed out\n", sdhc->name);
			ret = MMC_TIMEOUT;
			goto tuning_failed;
		}
	}

	if (!(ctrl & SDHCI_CTRL_TUNED_CLK)) {
		printf("%s: HW tuning failed\n", sdhc->name);
		ret = MMC_UNUSABLE_ERR;
		goto tuning_failed;
	}

	printf("%s: Tuning complete\n", sdhc->name);
	ret = 0;
	goto tuning_complete;

tuning_failed:
	/* Tuning has timed out or failed. */
	reg = sdhci_readw(sdhc, SDHCI_HOST_CONTROL2);
	reg &= ~SDHCI_CTRL_TUNED_CLK;
	reg &= ~SDHCI_CTRL_EXEC_TUNING;
	sdhci_writew(sdhc, reg, SDHCI_HOST_CONTROL2);

tuning_complete:
	media->ctrlr->bus_width = bus_width;
	media->ctrlr->set_ios(media->ctrlr);

	return ret;
}

SdhciHost *new_bayhub_sdhci_host(pcidev_t dev, int platform_info,
			      int clock_min, int clock_max)
{
	SdhciHost *host;
	host = new_pci_sdhci_host(dev, platform_info,
				clock_min, clock_max);
	host->mmc_ctrlr.execute_tuning = bh720_execute_tuning;

	return host;
}

/*
 * Copyright 2013 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>
#include <pci/pci.h>

#include "drivers/storage/sdhci.h"

#define SDHCI_NAME_LENGTH 20

#define PCI_CLASS_SYSTEM_PERIPHERAL		0x08
#define PCI_SUBCLASS_SD_HOST_CONTROLLER		0x05
#define PCI_SDHCI_PROG_IF_DMA			0x01

#define SDHCI_NAME_LENGTH			20

static bool is_sdhci_ctrlr(pcidev_t dev)
{
	const uint8_t class = pci_read_config8(dev, REG_CLASS);
	const uint8_t subclass = pci_read_config8(dev, REG_SUBCLASS);
	const uint8_t prog_if = pci_read_config8(dev, REG_PROG_IF);

	return class == PCI_CLASS_SYSTEM_PERIPHERAL &&
		subclass == PCI_SUBCLASS_SD_HOST_CONTROLLER &&
		prog_if == PCI_SDHCI_PROG_IF_DMA;
}

static int is_pci_bridge(pcidev_t dev)
{
	uint8_t header_type = pci_read_config8(dev, REG_HEADER_TYPE);
	header_type &= 0x7f;
	return header_type == HEADER_TYPE_BRIDGE;
}

/* Discover the register file address of the PCI SDHCI device. */
static int get_pci_bar(pcidev_t dev, uintptr_t *bar)
{
	uint32_t addr;

	addr = pci_read_config32(dev, PCI_BASE_ADDRESS_0);

	if (addr == ((uint32_t)~0))
		return -1;

	*bar = (uintptr_t)(addr & ~0xf);

	return 0;
}

/* Initialize an HDHCI port */
SdhciHost *new_pci_sdhci_host(pcidev_t dev, unsigned int platform_info,
			      int clock_min, int clock_max)
{
	SdhciHost *host;
	uintptr_t bar;

	if (get_pci_bar(dev, &bar)) {
		printf("Failed to get BAR for PCI SDHCI %d.%d.%d", PCI_BUS(dev),
		       PCI_SLOT(dev), PCI_FUNC(dev));
		return NULL;
	}

	host = new_mem_sdhci_host(bar, platform_info, clock_min, clock_max);

	host->name = xzalloc(SDHCI_NAME_LENGTH);

	snprintf(host->name, SDHCI_NAME_LENGTH, "PCI SDHCI %d.%d.%d",
		 PCI_BUS(dev), PCI_SLOT(dev), PCI_FUNC(dev));

	return host;
}

typedef struct sdhci_pci_host {
	SdhciHost host;
	pcidev_t dev;
	int (*update)(BlockDevCtrlrOps *me);

} SdhciPciHost;

static int sdhci_pci_init(BlockDevCtrlrOps *me) {

	SdhciPciHost *pci_host =
		container_of(me, SdhciPciHost, host.mmc_ctrlr.ctrlr.ops);
	SdhciHost *host = &pci_host->host;
	BlockDevCtrlr *block_ctrlr = &host->mmc_ctrlr.ctrlr;
	pcidev_t dev = pci_host->dev;
	uintptr_t bar;

	if (is_pci_bridge(dev)) {
		uint32_t bus = pci_read_config32(dev, REG_PRIMARY_BUS);
		bus = (bus >> 8) & 0xff;
		dev = PCI_DEV(bus, 0, 0);
	}

	if (!is_sdhci_ctrlr(dev)) {
		printf("No known SDHCI device found at %d:%d.%d", PCI_BUS(dev),
		       PCI_SLOT(dev), PCI_FUNC(dev));
		block_ctrlr->ops.update = NULL;
		block_ctrlr->need_update = 0;
		return -1;
	}

	printf("Found SDHCI at %d:%d.%d\n", PCI_BUS(dev), PCI_SLOT(dev),
	       PCI_FUNC(dev));

	if (get_pci_bar(dev, &bar)) {
		printf("Failed to get BAR for PCI SDHCI %d:%d.%d", PCI_BUS(dev),
		       PCI_SLOT(dev), PCI_FUNC(dev));
		block_ctrlr->ops.update = NULL;
		block_ctrlr->need_update = 0;
		return -1;
	}

	host->ioaddr = (void *)bar;
	host->name = xzalloc(SDHCI_NAME_LENGTH);

	snprintf(host->name, SDHCI_NAME_LENGTH, "PCI SDHCI %d:%d.%d",
		 PCI_BUS(dev), PCI_SLOT(dev), PCI_FUNC(dev));

	/*
	 * Replace the update method with the original so sdhci_pci_init never
	 * gets called again.
	 */
	block_ctrlr->ops.update = pci_host->update;
	pci_host->update = NULL;

	return block_ctrlr->ops.update(me);
}

SdhciHost *probe_pci_sdhci_host(pcidev_t dev, unsigned int platform_info)
{

	SdhciPciHost *pci_host;
	int removable = platform_info & SDHCI_PLATFORM_REMOVABLE;

	pci_host = xzalloc(sizeof(*pci_host));

	pci_host->dev = dev;
	pci_host->host.platform_info = platform_info;

	pci_host->host.mmc_ctrlr.slot_type =
		removable ? MMC_SLOT_TYPE_REMOVABLE : MMC_SLOT_TYPE_EMBEDDED;

	if (!removable)
		pci_host->host.mmc_ctrlr.hardcoded_voltage =
			OCR_HCS | MMC_VDD_165_195 | MMC_VDD_27_28 |
			MMC_VDD_28_29 | MMC_VDD_29_30 | MMC_VDD_30_31 |
			MMC_VDD_31_32 | MMC_VDD_32_33 | MMC_VDD_33_34 |
			MMC_VDD_34_35 | MMC_VDD_35_36;

	add_sdhci(&pci_host->host);

	/* We temporarily replace the sdhci_update call with the sdhci_pci_init
	 * call. */
	pci_host->update = pci_host->host.mmc_ctrlr.ctrlr.ops.update;
	pci_host->host.mmc_ctrlr.ctrlr.ops.update = sdhci_pci_init;

	/*
	 * We return SdhciHost because SdhciPciHost is an implementation detail
	 */
	return &pci_host->host;
}

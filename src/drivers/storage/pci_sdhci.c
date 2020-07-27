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
SdhciHost *new_pci_sdhci_host(pcidev_t dev, int platform_info, int clock_min,
			      int clock_max)
{
	SdhciHost *host;
	uintptr_t bar;
	int removable = platform_info & SDHCI_PLATFORM_REMOVABLE;

	if (get_pci_bar(dev, &bar)) {
		printf("Failed to get BAR for PCI SDHCI %d.%d.%d", PCI_BUS(dev),
		       PCI_SLOT(dev), PCI_FUNC(dev));
		return NULL;
	}

	host = xzalloc(sizeof(*host));
	host->name = xzalloc(SDHCI_NAME_LENGTH);

	snprintf(host->name, SDHCI_NAME_LENGTH, "PCI SDHCI %d.%d.%d",
		 PCI_BUS(dev), PCI_SLOT(dev), PCI_FUNC(dev));

	host->quirks = SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER;

	if (platform_info & SDHCI_PLATFORM_NO_EMMC_HS200)
		host->quirks |= SDHCI_QUIRK_NO_EMMC_HS200;

	if (platform_info & SDHCI_PLATFORM_SUPPORTS_HS400ES)
		host->quirks |= SDHCI_QUIRK_SUPPORTS_HS400ES;

	if (platform_info & SDHCI_PLATFORM_EMMC_1V8_POWER)
		host->quirks |= SDHCI_QUIRK_EMMC_1V8_POWER;

	if (platform_info & SDHCI_PLATFORM_CLEAR_TRANSFER_BEFORE_CMD)
		host->quirks |=
			SDHCI_QUIRK_CLEAR_TRANSFER_BEFORE_CMD;

	host->clock_f_min = clock_min;
	host->clock_f_max = clock_max;
	host->removable = removable;
	host->ioaddr = (void *)bar;

	if (!removable)
		/*
		 * The value translates to 'block access mode, supporting
		 * 1.7..1.95 and 2.7..3.6 voltage ranges, which is typical for
		 * eMMC devices.
		 */
		host->mmc_ctrlr.hardcoded_voltage = 0x40ff8080;

	add_sdhci(host);

	return host;
}

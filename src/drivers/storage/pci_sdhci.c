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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>
#include <pci/pci.h>

#include "drivers/storage/sdhci.h"

typedef struct {
	SdhciHost sdhci_host;
	pcidev_t sdhci_dev;
	char dev_name[0x20];
} PciSdhciHost;

/* Discover the register file address of the PCI SDHCI device. */
static int attach_device(SdhciHost *host)
{
	PciSdhciHost *pci_host;
	uint32_t addr;

	pci_host = container_of(host, PciSdhciHost, sdhci_host);
	addr = pci_read_config32(pci_host->sdhci_dev, PCI_BASE_ADDRESS_0);

	if (addr == ((uint32_t)~0)) {
		printf("%s: Error: %s not found\n",
		       __func__, pci_host->dev_name);
		return -1;
	}

	host->ioaddr = (void *) (addr & ~0xf);

	return 0;
}

/* Initialize an HDHCI port */
SdhciHost *new_pci_sdhci_host(pcidev_t dev, int platform_info,
			      int clock_min, int clock_max)
{
	PciSdhciHost *host;
	int removable = platform_info & SDHCI_PLATFORM_REVOMVABLE;

	host = xzalloc(sizeof(*host));

	host->sdhci_dev = dev;
	snprintf(host->dev_name, sizeof(host->dev_name), "PCI SDHCI %d.%d.%d",
		 PCI_BUS(dev), PCI_SLOT(dev), PCI_FUNC(dev));

	host->sdhci_host.quirks = SDHCI_QUIRK_NO_HISPD_BIT |
		SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER;

	if (platform_info & SDHCI_PLATFORM_NO_EMMC_HS200)
		host->sdhci_host.quirks |= SDHCI_QUIRK_NO_EMMC_HS200;

	if (platform_info & SDHCI_PLATFORM_EMMC_1V8_POWER)
		host->sdhci_host.quirks |= SDHCI_QUIRK_EMMC_1V8_POWER;

	host->sdhci_host.attach = attach_device;
	host->sdhci_host.clock_f_min = clock_min;
	host->sdhci_host.clock_f_max = clock_max;
	host->sdhci_host.removable = removable;

	/*
	 * The value translates to 'block access mode, supporting 1.7..1.95
	 * and 2.7..3.6 voltage ranges, which is typical for eMMC devices.
	 */
	host->sdhci_host.mmc_ctrlr.hardcoded_voltage = 0x40ff8080;
	/*
	 * Need to program host->ioaddr for SD/MMC read/write operation
	 */
	attach_device(&host->sdhci_host);

	add_sdhci(&host->sdhci_host);

	return &host->sdhci_host;
}

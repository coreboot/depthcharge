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

#include "sdhci.h"

#define IF_NAME_SIZE	0x20

/* Initialize an HDHCI port */
SdhciHost *new_pci_sdhci_host(pcidev_t dev, int removable,
			      int clock_min, int clock_max)
{
	SdhciHost *host;

	host = (SdhciHost *)malloc(sizeof(SdhciHost) + IF_NAME_SIZE);
	if (!host) {
		printf("%s: sdhci__host malloc failed!\n", __func__);
		return NULL;
	}

	/* the name area does not have to be zeroed */
	memset(host, 0, sizeof(*host));

	host->name = (void *)(host + 1);
	snprintf(host->name, IF_NAME_SIZE, "PCI SDHCI %d.%d.%d",
		 PCI_BUS(dev), PCI_SLOT(dev), PCI_FUNC(dev));

	host->ioaddr = (void *) (pci_read_config32(dev, PCI_BASE_ADDRESS_0) &
				 ~0xf);

	host->quirks = SDHCI_QUIRK_NO_HISPD_BIT |
		SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER;

	host->version = sdhci_readw(host, SDHCI_HOST_VERSION) & 0xff;

	host->removable = removable;

	if (add_sdhci(host, clock_max, clock_min))
		return NULL;

	/*
	 * The value translates to 'block access mode, supporting 1.7..1.95
	 * and 2.7..3.6 voltage ranges, which is typical for eMMC devices.
	 */
	host->mmc_ctrlr.hardcoded_voltage = 0x40ff8080;

	return host;
}

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

#include "drivers/storage/sdhci.h"

/* Initialize an SDHCI port with memory address */
SdhciHost *new_mem_sdhci_host(void *ioaddr, int platform_info,
			      int clock_min, int clock_max)
{
	SdhciHost *host;
	int removable = platform_info & SDHCI_PLATFORM_REVOMVABLE;

	host = xzalloc(sizeof(*host));

	host->quirks = SDHCI_QUIRK_NO_HISPD_BIT |
		SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER;

	if (platform_info & SDHCI_PLATFORM_NO_EMMC_HS200)
		host->quirks |= SDHCI_QUIRK_NO_EMMC_HS200;

	if (platform_info & SDHCI_PLATFORM_EMMC_1V8_POWER)
		host->quirks |= SDHCI_QUIRK_EMMC_1V8_POWER;

	host->clock_f_min = clock_min;
	host->clock_f_max = clock_max;
	host->removable = removable;
	host->ioaddr = ioaddr;

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

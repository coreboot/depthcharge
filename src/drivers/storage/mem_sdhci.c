/*
 * Copyright 2013 Google LLC
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

#include "drivers/storage/sdhci.h"

/* Initialize an SDHCI port with memory address */
SdhciHost *new_mem_sdhci_host(uintptr_t ioaddr, unsigned int platform_info,
			      int clock_min, int clock_max)
{
	SdhciHost *host;
	int removable = platform_info & SDHCI_PLATFORM_REMOVABLE;

	host = xzalloc(sizeof(*host));

	host->platform_info = platform_info;

	host->quirks = SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER;

	host->clock_f_min = clock_min;
	host->clock_f_max = clock_max;
	host->ioaddr = (void *)ioaddr;

	/* TODO: Can we use the SDHCI SLOT register instead? */
	host->mmc_ctrlr.slot_type =
		removable ? MMC_SLOT_TYPE_REMOVABLE : MMC_SLOT_TYPE_EMBEDDED;

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

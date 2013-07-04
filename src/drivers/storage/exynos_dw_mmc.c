/*
 * (C) Copyright 2012 SAMSUNG Electronics
 * Jaehoon Chung <jh80.chung@samsung.com>
 * Copyright 2013 Google Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,  MA 02111-1307 USA
 *
 */

#include <libpayload.h>
#include <stdint.h>

#include "base/init_funcs.h"
#include "config.h"
#include "drivers/storage/dw_mmc.h"

#define DWMCI_CLKSEL		0x09C
#define DWMCI_SHIFT_0		0x0
#define DWMCI_SHIFT_1		0x1
#define DWMCI_SHIFT_2		0x2
#define DWMCI_SHIFT_3		0x3
#define DWMCI_SET_SAMPLE_CLK(x)	(x)
#define DWMCI_SET_DRV_CLK(x)	((x) << 16)
#define DWMCI_SET_DIV_RATIO(x)	((x) << 24)

#define	DWMMC_MAX_FREQ			52000000
#define	DWMMC_MIN_FREQ			400000

typedef struct {
	void *addr;
	uint32_t sclk;  /* sclk is setup by Coreboot. */
	int bus_width;
	int removable;
	uint32_t timing[3];
} ExynosDwMmcConfig;

/*
 * Function used as callback function to initialise the
 * CLKSEL register for every mmc channel.
 */
static void exynos_dwmci_clksel(DwmciHost *host)
{
	dwmci_writel(host, DWMCI_CLKSEL, host->clksel_val);
}

static int exynos_dwmci_is_card_present(MmcDevice *mmc)
{
	DwmciHost *host = (DwmciHost *)mmc->host;
	if (!mmc->block_dev->removable)
		return 1;
	/* CDETECT is Low-active. */
	return !dwmci_readl(host, DWMCI_CDETECT);
}

int exynos_dwmmc_initialize(int index, const ExynosDwMmcConfig *config,
			    MmcDevice **refresh_list)
{
	DwmciHost *host = NULL;
	uint32_t clksel_val;
	host = malloc(sizeof(*host));
	if (!host) {
		printf("dwmci_host malloc fail!\n");
		return 1;
	}
	memset(host, 0, sizeof(*host));

	host->name = "EXYNOS DWMMC";
	host->dev_index = index;
	host->ioaddr = config->addr;
	host->buswidth = config->bus_width;

	clksel_val = (DWMCI_SET_SAMPLE_CLK(config->timing[0]) |
		      DWMCI_SET_DRV_CLK(config->timing[1]) |
		      DWMCI_SET_DIV_RATIO(config->timing[2]));
	host->clksel_val = clksel_val;

	host->bus_hz = config->sclk;
	host->clksel = exynos_dwmci_clksel;

	/* Add the mmc channel to be registered with mmc core */
	if (dw_mmc_register(host, DWMMC_MAX_FREQ, DWMMC_MIN_FREQ,
			    config->removable, refresh_list,
			    exynos_dwmci_is_card_present) != 0) {
		mmc_debug("dwmmc%d registration failed\n", index);
		return -1;
	}
	return 0;
}

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
#define	DWMMC_MMC0_CLKSEL_VAL		0x03030001
#define	DWMMC_MMC2_CLKSEL_VAL		0x03020001

/*
 * Function used as callback function to initialise the
 * CLKSEL register for every mmc channel.
 */
static void exynos_dwmci_clksel(DwmciHost *host)
{
	dwmci_writel(host, DWMCI_CLKSEL, host->clksel_val);
}

unsigned int exynos_dwmci_get_clk(int dev_index)
{
	// TODO(hungte) Implement get_mmc_clk or remove it.
	// return get_mmc_clk(dev_index);
	return 0;
}

static int exynos_dwmci_is_card_present(MmcDevice *mmc)
{
	DwmciHost *host = (DwmciHost *)mmc->host;
	if (!mmc->block_dev->removable)
		return 1;
	/* CDETECT is Low-active. */
	return !dwmci_readl(host, DWMCI_CDETECT);
}

/*
 * This function adds the mmc channel to be registered with mmc core.
 * index -	mmc channel number.
 * regbase -	register base address of mmc channel specified in 'index'.
 * bus_width -	operating bus width of mmc channel specified in 'index'.
 * clksel -	value to be written into CLKSEL register in case of FDT.
 *		NULL in case od non-FDT.
 * removable - set to True if the device can be removed (like an SD card), to
 *             False if not (like ak eMMC drive)
 * pre_init -	Kick the mmc on startup so that it is ready sooner when we
 *		need it
 */
int exynos_dwmci_add_port(int index, uint32_t regbase, int bus_width,
			  uint32_t clksel, int removable, int pre_init)
{
	DwmciHost *host = NULL;
	unsigned int div;
	unsigned long freq, sclk;
	host = malloc(sizeof(*host));
	if (!host) {
		printf("dwmci_host malloc fail!\n");
		return 1;
	}
	/* request mmc clock vlaue of 52MHz.  */
	freq = 52000000;
	// TODO(hungte) Implement get_mmc_clk or remove it.
#if 0
	sclk = get_mmc_clk(index);
#else
	sclk = 0;
#endif
	div = (sclk + freq - 1) / freq;

	/* set the clock divisor for mmc */
	// TODO(hungte) Implement set_mmc_clk or remove it.
#if 0
	set_mmc_clk(index, div);
#else
	printf("%d\n", div);
#endif

	host->name = "EXYNOS DWMMC";
	host->ioaddr = (void *)regbase;
	host->buswidth = bus_width;

	if (clksel) {
		host->clksel_val = clksel;
	} else {
		if (0 == index)
			host->clksel_val = DWMMC_MMC0_CLKSEL_VAL;
		if (2 == index)
			host->clksel_val = DWMMC_MMC2_CLKSEL_VAL;
	}

	host->clksel = exynos_dwmci_clksel;
	host->dev_index = index;
	host->mmc_clk = exynos_dwmci_get_clk;

	/* Add the mmc channel to be registered with mmc core */
	if (dw_mmc_register(host, DWMMC_MAX_FREQ, DWMMC_MIN_FREQ, removable,
			    NULL, exynos_dwmci_is_card_present) != 0) {
		mmc_debug("dwmmc%d registration failed\n", index);
		return -1;
	}
	return 0;
}

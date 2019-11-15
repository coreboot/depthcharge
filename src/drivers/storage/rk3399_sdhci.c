/*
 * Copyright 2016 Rockchip Electronics Co., Ltd.
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

#include <arch/io.h>
#include "drivers/storage/rk_sdhci.h"

struct rk3399_grf_emmc_phy {
	u32 emmcphy_con[7];
	u32 reserved;
	u32 emmcphy_status;
};

#define RK_CLRSETBITS(clr, set) ((((clr) | (set)) << 16) | set)

static struct rk3399_grf_emmc_phy *emmc_phy = (void *)0xff77f780;

#define PHYCTRL_CALDONE_MASK		0x1
#define PHYCTRL_CALDONE_SHIFT		0x6
#define PHYCTRL_CALDONE_DONE		0x1

#define PHYCTRL_DLLRDY_MASK		0x1
#define PHYCTRL_DLLRDY_SHIFT		0x5
#define PHYCTRL_DLLRDY_DONE		0x1

#define PHYCTRL_FREQSEL_200M            0x0
#define PHYCTRL_FREQSEL_50M             0x1
#define PHYCTRL_FREQSEL_100M            0x2
#define PHYCTRL_FREQSEL_150M            0x3

static void rk3399_emmc_phy_power_on(uint32_t clock)
{
	u32 caldone, dllrdy, freqsel;
	uint64_t start_us;

	writel(RK_CLRSETBITS(7 << 4, 0), &emmc_phy->emmcphy_con[6]);
	writel(RK_CLRSETBITS(1 << 11, 1 << 11), &emmc_phy->emmcphy_con[0]);
	writel(RK_CLRSETBITS(0xf << 7, 4 << 7), &emmc_phy->emmcphy_con[0]);

	/*
	 * According to the user manual, calpad calibration
	 * cycle takes more than 2us without the minimal recommended
	 * value, so we may need a little margin here
	 */
	udelay(3);
	writel(RK_CLRSETBITS(1, 1), &emmc_phy->emmcphy_con[6]);

	/*
	 * According to the user manual, it asks driver to wait 5us for
	 * calpad busy trimming. However it is documented that this value is
	 * PVT(A.K.A process,voltage and temperature) relevant, so some
	 * failure cases are found which indicates we should be more tolerant
	 * to calpad busy trimming.
	 */
	start_us = timer_us(0);
	do {
		udelay(1);
		caldone = readl(&emmc_phy->emmcphy_status);
		caldone = (caldone >> PHYCTRL_CALDONE_SHIFT) &
			PHYCTRL_CALDONE_MASK;
		if (caldone == PHYCTRL_CALDONE_DONE)
			break;
	} while (timer_us(start_us) < 50);
	if (caldone != PHYCTRL_CALDONE_DONE) {
		printf("%s: caldone timeout.\n", __func__);
		return;
	}

	/* Set the frequency of the DLL operation */
	if (clock < 75 * MHz)
		freqsel = PHYCTRL_FREQSEL_50M;
	else if (clock < 125 * MHz)
		freqsel = PHYCTRL_FREQSEL_100M;
	else if (clock < 175 * MHz)
		freqsel = PHYCTRL_FREQSEL_150M;
	else
		freqsel = PHYCTRL_FREQSEL_200M;

	writel(RK_CLRSETBITS(3 << 12, freqsel << 12),
	       &emmc_phy->emmcphy_con[0]);
	writel(RK_CLRSETBITS(1 << 1, 1 << 1), &emmc_phy->emmcphy_con[6]);

	start_us = timer_us(0);

	do {
		udelay(1);
		dllrdy = readl(&emmc_phy->emmcphy_status);
		dllrdy = (dllrdy >> PHYCTRL_DLLRDY_SHIFT) & PHYCTRL_DLLRDY_MASK;
		if (dllrdy == PHYCTRL_DLLRDY_DONE)
			break;
	} while (timer_us(start_us) < 50000);

	if (dllrdy != PHYCTRL_DLLRDY_DONE)
		printf("%s: dllrdy timeout.\n", __func__);

}

static void rk3399_emmc_phy_power_off(void)
{
	writel(RK_CLRSETBITS(1, 0), &emmc_phy->emmcphy_con[6]);
	writel(RK_CLRSETBITS(1 << 1, 0), &emmc_phy->emmcphy_con[6]);
}

#define SDHCI_ARASAN_VENDOR_REGISTER	0x78
#define VENDOR_ENHANCED_STROBE		(1 << 0)

static void rk3399_sdhci_set_ios(MmcCtrlr *mmc_ctrlr)
{
	u32 vendor;
	SdhciHost *host = container_of(mmc_ctrlr,
				       SdhciHost, mmc_ctrlr);
	int cycle_phy = mmc_ctrlr->bus_hz != host->clock &&
			mmc_ctrlr->bus_hz > 400*KHz;

	if (cycle_phy)
		rk3399_emmc_phy_power_off();

	sdhci_set_ios(mmc_ctrlr);

	/* configure HS400 Enhanced Store feature */
	vendor = sdhci_readl(host, SDHCI_ARASAN_VENDOR_REGISTER);
	if (mmc_ctrlr->timing == MMC_TIMING_MMC_HS400ES)
		vendor |= VENDOR_ENHANCED_STROBE;
	else
		vendor &= ~VENDOR_ENHANCED_STROBE;
	sdhci_writel(host, vendor, SDHCI_ARASAN_VENDOR_REGISTER);

	if (cycle_phy)
		rk3399_emmc_phy_power_on(mmc_ctrlr->bus_hz);
}

SdhciHost *new_rk_sdhci_host(uintptr_t ioaddr, int platform_info,
			      int clock_min, int clock_max, int clock_base)
{
	SdhciHost *host;
	host = new_mem_sdhci_host(ioaddr, platform_info,
				clock_min, clock_max, clock_base);
	host->mmc_ctrlr.set_ios = &rk3399_sdhci_set_ios;

	return host;
}

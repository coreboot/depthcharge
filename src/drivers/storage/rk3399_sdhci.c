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

static void rk3399_emmc_phy_power_on(void)
{
	u32 caldone, dllrdy;
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
	 * According to the user manual, it asks driver to
	 * wait 5us for calpad busy trimming
	 */
	udelay(5);
	caldone = readl(&emmc_phy->emmcphy_status);
	caldone = (caldone >> PHYCTRL_CALDONE_SHIFT) & PHYCTRL_CALDONE_MASK;
	if (caldone != PHYCTRL_CALDONE_DONE) {
		printf("%s: caldone timeout.\n", __func__);
		return;
	}
	/* Set the frequency of the DLL operation */
	writel(RK_CLRSETBITS(3 << 12, 3 << 12), &emmc_phy->emmcphy_con[0]);
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

static void rk3399_sdhci_set_ios(MmcCtrlr *mmc_ctrlr)
{
	SdhciHost *host = container_of(mmc_ctrlr,
				       SdhciHost, mmc_ctrlr);
	int change_clock = mmc_ctrlr->bus_hz != host->clock;

	if (change_clock)
		rk3399_emmc_phy_power_off();

	sdhci_set_ios(mmc_ctrlr);

	if (change_clock)
		rk3399_emmc_phy_power_on();
}

SdhciHost *new_rk_sdhci_host(void *ioaddr, int platform_info,
			      int clock_min, int clock_max, int clock_base)
{
	SdhciHost *host;
	host = new_mem_sdhci_host(ioaddr, platform_info,
				clock_min, clock_max, clock_base);
	host->mmc_ctrlr.set_ios = &rk3399_sdhci_set_ios;

	return host;
}

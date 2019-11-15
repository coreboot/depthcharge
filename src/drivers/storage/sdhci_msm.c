/*
 * Copyright (C) 2019, The Linux Foundation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>

#include "drivers/storage/sdhci_msm.h"

/*
 * msm specific SDHC initialization
 */
static int sdhci_msm_init(SdhciHost *host)
{
	u32 vendor_caps = 0, config = 0;

	mmc_debug("Initializing SDHCI MSM host controller!\n")

	/* Read host controller capabilities */
	vendor_caps = sdhci_readl(host, SDHCI_CAPABILITIES);

	/* Fix the base clock to max clock supported */
	vendor_caps &= ~SDHCI_CLOCK_V3_BASE_MASK;
	vendor_caps |= (host->clock_f_max/(1*MHz)) << SDHCI_CLOCK_BASE_SHIFT;

	/*
	 * Explicitly enable the capabilities which are not advertised
	 * by default
	 */
	if (host->removable)
		vendor_caps |= SDHCI_CAN_VDD_300;
	else
		vendor_caps |= SDHCI_CAN_VDD_180 | SDHCI_CAN_DO_8BIT;

	/*
	 * Update internal capabilities register so that these updated values
	 * will get reflected in SDHCI_CAPABILITEIS register.
	 */
	sdhci_writel(host, vendor_caps, SDCC_HC_VENDOR_SPECIFIC_CAPABILITIES0);

	/* Set the PAD_PWR_SWITCH_EN bit. And set it based on host capability */
	config = VENDOR_SPEC_FUN1_POR_VAL | HC_IO_PAD_PWR_SWITCH_EN;
	if (vendor_caps & SDHCI_CAN_VDD_300)
		config &= ~HC_IO_PAD_PWR_SWITCH;
	else if (vendor_caps & SDHCI_CAN_VDD_180)
		config |= HC_IO_PAD_PWR_SWITCH;

	/*
	 * Disable HC_SELECT_IN to be able to use the UHS mode select
	 * configuration from HOST_CONTROL2 register for all other modes.
	 *
	 * For disabling it, Write 0 to HC_SELECT_IN and HC_SELECT_IN_EN
	 * fields in VENDOR_SPEC_FUNC1 register.
	 */
	config &= ~HC_SELECT_IN_EN & ~HC_SELECT_IN_MASK;
	sdhci_writel(host, config, SDCC_HC_VENDOR_SPECIFIC_FUNC1);

	/*
	 * Reset the vendor spec register to power on reset state.
	 * This is to ensure that this register is set to right value
	 * incase if this register get updated by bootrom when using SDHCI boot.
	 */
	sdhci_writel(host, VENDOR_SPEC_FUN3_POR_VAL,
		     SDCC_HC_VENDOR_SPECIFIC_FUNC3);

	/*
	 * Set SD power off, otherwise reset will result in pwr irq.
	 * And without setting bus off status, reset would fail.
	 */
	sdhci_writeb(host, 0x0, SDHCI_POWER_CONTROL);
	udelay(10);

	return 0;
}


/*
 * This function will get invoked from board_setup()
 */
SdhciHost *new_sdhci_msm_host(uintptr_t ioaddr, int platform_info, int clock_max,
			      uintptr_t tlmmAddr, GpioOps *cd_gpio)
{
	SdhciHost *host;

	mmc_debug("Max-Clock = %dMHz, Platform-Info = 0x%08x\n",
		  clock_max/MHz, platform_info);

	host = new_mem_sdhci_host(ioaddr, platform_info, 400*KHz, clock_max, 0);

	host->attach = sdhci_msm_init;
	host->cd_gpio = cd_gpio;

	/* We don't need below quirk, so clear it */
	host->quirks &= ~SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER;

	/* Configure drive strengths of interface lines */
	if (host->removable)
		writel(SDC2_TLMM_CONFIG, (void *)tlmmAddr);
	else
		writel(SDC1_TLMM_CONFIG, (void *)tlmmAddr);

	return host;
}

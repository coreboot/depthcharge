/*
 * Copyright (C) 2019-20, The Linux Foundation.  All rights reserved.
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
 * Initialize the DLL (Programmable Delay Line)
 */
static void sdhci_msm_init_dll(SdhciHost *host)
{
	u32 config = 0, mclk_freq = 0;
	u8 clk_cycles = 0;

	/* Disable DLL output clock  */
	config = sdhci_readl(host, SDCC_HC_REG_DLL_CONFIG);
	config &= ~CLK_OUT_EN;
	sdhci_writel(host, config, SDCC_HC_REG_DLL_CONFIG);

	/* Reset & Power down the DLL */
	config = sdhci_readl(host, SDCC_HC_REG_DLL_CONFIG);
	config |= DLL_RST;
	sdhci_writel(host, config, SDCC_HC_REG_DLL_CONFIG);

	config = sdhci_readl(host, SDCC_HC_REG_DLL_CONFIG);
	config |= DLL_PDN;
	sdhci_writel(host, config, SDCC_HC_REG_DLL_CONFIG);

	/* Program MCLK frequencey */
	config = sdhci_readl(host, SDCC_HC_REG_DLL_CONFIG_2);
	if (config & FLL_CYCLE_CNT)
		clk_cycles = 8;
	else
		clk_cycles = 4;
	mclk_freq = div_round_up((host->clock * clk_cycles),  XO_CLK);

	config &= ~MCLK_FREQ_CALC_MASK;
	config |= mclk_freq << MCLK_FREQ_CALC_SHIFT;
	sdhci_writel(host, config, SDCC_HC_REG_DLL_CONFIG_2);
	udelay(5);

	/* Bring DLL out of reset and power down state */
	config = sdhci_readl(host, SDCC_HC_REG_DLL_CONFIG);
	config &= ~DLL_RST;
	sdhci_writel(host, config, SDCC_HC_REG_DLL_CONFIG);

	config = sdhci_readl(host, SDCC_HC_REG_DLL_CONFIG);
	config &= ~DLL_PDN;
	sdhci_writel(host, config, SDCC_HC_REG_DLL_CONFIG);

	/* Enable DLL Clock */
	config = sdhci_readl(host, SDCC_HC_REG_DLL_CONFIG_2);
	config &= ~DLL_CLOCK_DISABLE;
	sdhci_writel(host, config, SDCC_HC_REG_DLL_CONFIG_2);

	/* Enable DLL function */
	config = sdhci_readl(host, SDCC_HC_REG_DLL_CONFIG);
	config |= DLL_EN;
	sdhci_writel(host, config, SDCC_HC_REG_DLL_CONFIG);

	/* Enable DLL output clock  */
	config = sdhci_readl(host, SDCC_HC_REG_DLL_CONFIG);
	config |= CLK_OUT_EN;
	sdhci_writel(host, config, SDCC_HC_REG_DLL_CONFIG);
}


/* Calibrate the DLL for HS400 operation. */
static void sdhci_msm_cm_dll_sdc4_calibration(SdhciHost *host)
{
	u32 config = 0;
	uint64_t start = 0;

	/* Program DDR_CONFIG with POR value */
	sdhci_writel(host, DDR_CONFIG_POR_VAL, SDCC_HC_REG_DDR_CONFIG);

	/* Enable CMD input sampling with RCLK */
	config = sdhci_readl(host, SDCC_HC_VENDOR_SPECIFIC_DDR200_CFG);
	config |= CMDIN_RCLK_EN;
	sdhci_writel(host, config, SDCC_HC_VENDOR_SPECIFIC_DDR200_CFG);

	/* Calibrate the DLL */
	config = sdhci_readl(host, SDCC_HC_REG_DLL_CONFIG_2);
	config |= DDR_CAL_EN;
	sdhci_writel(host, config, SDCC_HC_REG_DLL_CONFIG_2);

	/* Wait until DLL locks for HS400 operation */
	start = timer_us(0);
	while (sdhci_readl(host, SDCC_HC_REG_DLL_STATUS) & DDR_DLL_LOCK_JDR) {
		if (timer_us(start) >= 10000) {
			printf("%s HS400 DLL failed to lock\n", __func__);
			return;
		}
		udelay(10);
	}
}

/* set_ios */
static void sdhci_msm_set_ios(MmcCtrlr *mmc_ctrlr)
{
	u32 config = 0;
	SdhciHost *host = container_of(mmc_ctrlr,
				       SdhciHost, mmc_ctrlr);

	sdhci_set_ios(mmc_ctrlr);

	config = sdhci_readl(host, SDCC_HC_VENDOR_SPECIFIC_FUNC1);
	/*
	 * Set HC_SELECT_IN_EN with SELECT_IN=0x6 for HS400 mode.
	 * This is to enable RCLK for data sampling.
	 * For rest of the speed modes let the controller use the
	 * 'UHS mode select' configuration from SDHC HOST_CONTROL2 register.
	 *
	 * Also configure dll input clock as MCLK/2 for HS400.
	 */
	config &= ~HC_SELECT_IN_EN & ~HC_SELECT_IN_MASK & ~HC_MCLK_SEL_MASK;
	if (mmc_ctrlr->timing == MMC_TIMING_MMC_HS400ES)
		config |= HC_SELECT_IN_HS400 | HC_SELECT_IN_EN |
			  HC_MCLK_SEL_HS400;
	sdhci_writel(host, config, SDCC_HC_VENDOR_SPECIFIC_FUNC1);

	if ((mmc_ctrlr->timing == MMC_TIMING_MMC_HS400ES) &&
	    (mmc_ctrlr->bus_hz == MMC_CLOCK_200MHZ )) {
		sdhci_msm_init_dll (host);
		sdhci_msm_cm_dll_sdc4_calibration(host);
	}
}


/*
 * msm specific SDHC initialization
 */
static int sdhci_msm_init(SdhciHost *host)
{
	u32 vendor_caps = 0, config = 0;

	mmc_debug("Initializing SDHCI MSM host controller!\n")

	/* Read host controller capabilities */
	vendor_caps = sdhci_readl(host, SDHCI_CAPABILITIES);

	/*
	 * Explicitly enable the capabilities which are not advertised
	 * by default
	 */
	if (host->mmc_ctrlr.slot_type == MMC_SLOT_TYPE_REMOVABLE)
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

	/* Write DLL registers with POR values */
	if (host->quirks & SDHCI_QUIRK_SUPPORTS_HS400ES) {
		sdhci_writel(host, config, SDCC_HC_REG_DLL_CONFIG);
		sdhci_writel(host, config, SDCC_HC_REG_DLL_CONFIG_2);
	}

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
	/*
	 * The base clock frequency field for SDHC Capability register can't be
	 * used for base clock > 255Mhz. On Qcom SDHC, base clock needed for
	 * HS400 mode is 384Mhz. So set no_clk_base platform flag irrespective
	 * of speed mode. This is a common property applicable to all targets.
	 * So set it here instead of passing from board file of every platform.
	 */
	platform_info |= SDHCI_PLATFORM_NO_CLK_BASE;

	host = new_mem_sdhci_host(ioaddr, platform_info, 400*KHz, clock_max,
					clock_max/MHz);

	host->attach = sdhci_msm_init;
	host->cd_gpio = cd_gpio;

	/* We don't need below quirk, so clear it */
	host->quirks &= ~SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER;

	/* Qcom SDHC needs double the input clock for DDR modes */
	host->quirks |= SDHCI_QUIRK_NEED_2X_CLK_FOR_DDR_MODE;

	/* Assign our own callback for set_ios */
	host->mmc_ctrlr.set_ios = &sdhci_msm_set_ios;

	/* Configure drive strengths of interface lines */
	if (host->mmc_ctrlr.slot_type == MMC_SLOT_TYPE_REMOVABLE)
		writel(SDC2_TLMM_CONFIG, (void *)tlmmAddr);
	else
		writel(SDC1_TLMM_CONFIG, (void *)tlmmAddr);

	return host;
}

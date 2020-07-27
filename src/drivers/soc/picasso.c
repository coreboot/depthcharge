// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "drivers/soc/picasso.h"
#include "drivers/storage/sdhci.h"

/* EMMCCFG is located 0x800 after EMMCHC, so these are EMMCCFG registers */
#define  EMMC_DLL_CFG0                  0x908
#define  EMMC_DLL_CFG1                  0x90A
#define   EMMC_DLL_SOFT_RESET           BIT(1)
#define   EMMC_DLL_ENABLE               BIT(0)
#define  EMMC_DLL_CFG2                  0x90C
#define   EMMC_DLL_O_LOCK_VALUE_SHIFT   8
#define   EMMC_DLL_O_LOCK               BIT(0)

static void enable_emmc_dll(SdhciHost *host)
{
	u16 reg;

	reg = sdhci_readw(host, EMMC_DLL_CFG1);
	if (reg & EMMC_DLL_ENABLE)
		return;

	/* These values are all copied from the linux kernel */
	/* I_PARAM_INCREMENT: 0x32, I_PARAM_START_POINT: 0x10 */
	sdhci_writew(host, 0x3210, EMMC_DLL_CFG0);

	/* I_DELAY: 0x40 */
	sdhci_writew(host, 0x4000, EMMC_DLL_CFG1);

	/* Not sure why a delay is required */
	udelay(20);

	/* I_DELAY: 0x40 */
	sdhci_writew(host, 0x4000 | EMMC_DLL_SOFT_RESET | EMMC_DLL_ENABLE,
		     EMMC_DLL_CFG1);

	do {
		reg = sdhci_readw(host, EMMC_DLL_CFG2);
	} while (!(reg & EMMC_DLL_O_LOCK));
}

void emmc_set_ios(MmcCtrlr *mmc_ctrlr)
{
	u16 reg;
	SdhciHost *host = container_of(mmc_ctrlr,
				       SdhciHost, mmc_ctrlr);

	unsigned int orig_clock = host->clock;

	sdhci_set_ios(mmc_ctrlr);

	/*
	 * The AMD eMMC Controller can only use the tuned clock at the tuned
	 * frequency. If we change frequencies we must disable the tuned clock.
	 * If the clock is then changed back to the tuned frequency then we
	 * are allowed to re-enable the tuned clock without re-tuning.
	 */
	if (orig_clock != host->clock && host->tuned_clock) {
		if (host->tuned_clock != host->clock) {
			printf("%s: Disabling Tuning\n", __func__);
			reg = sdhci_readw(host, SDHCI_HOST_CONTROL2);
			reg &= ~SDHCI_CTRL_TUNED_CLK;
			sdhci_writew(host, reg, SDHCI_HOST_CONTROL2);
		} else {
			printf("%s: Re-Enabling Tuning\n", __func__);
			reg = sdhci_readw(host, SDHCI_HOST_CONTROL2);
			reg |= SDHCI_CTRL_TUNED_CLK;
			sdhci_writew(host, reg, SDHCI_HOST_CONTROL2);
		}

	}

	/* eMMC DLL is only required for HS400 */
	if (mmc_ctrlr->timing == MMC_TIMING_MMC_HS400)
		enable_emmc_dll(host);
}

int emmc_supports_hs400(void)
{
	u32 reg = readl(EMMCHC + SDHCI_CAPABILITIES_1);

	/*
	 * There is no standard way of describing HS400 support. AMD FSP uses
	 * the DDR50 flag as an indicator to signal HS400 support.
	 */
	if (reg & SDHCI_SUPPORT_DDR50)
		return 1;

	return 0;
}

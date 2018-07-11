/*
 * Copyright (C) 2018 Intel Corporation
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

/*
 * These needs to be included first.
 * Some of the driver headers would be dependent on these.
 */
#include <arch/io.h>
#include <pci.h>
#include <pci/pci.h>
#include <libpayload.h>
#include <sysinfo.h>
#include "base/init_funcs.h"
#include "base/list.h"
#include "config.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/spi/intel_gspi.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/icelake.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/icelake.h"
#include "drivers/sound/max98373.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/spi.h"
#include "drivers/tpm/tpm.h"

/*
 * Clock frequencies for the eMMC and SD ports are defined below. The minimum
 * frequency is the same for both interfaces, the firmware does not run any
 * interface faster than 52 MHz, but defines maximum eMMC frequency as 200 MHz
 * for proper divider settings.
 */
#define EMMC_SD_CLOCK_MIN	400000
#define EMMC_CLOCK_MAX		200000000
#define SD_CLOCK_MAX		52000000

static int cr50_irq_status(void)
{
	return icelake_get_gpe(GPE0_DW1_16 ); /* GPP_D16 */
}

static void dragonegg_setup_tpm(void)
{
	if (IS_ENABLED(CONFIG_DRIVER_TPM_SPI)) {
		/* SPI TPM */
		const IntelGspiSetupParams gspi0_params = {
			.dev = PCI_DEV(0, 0x1e, 2),
			.cs_polarity = SPI_POLARITY_LOW,
			.clk_phase = SPI_CLOCK_PHASE_FIRST,
			.clk_polarity = SPI_POLARITY_LOW,
			.ref_clk_mhz = 120,
			.gspi_clk_mhz = 1,
		};
		tpm_set_ops(&new_tpm_spi(new_intel_gspi(&gspi0_params),
					 cr50_irq_status)->ops);
	}
}

static void dragonegg_config_i2s_audio(void)
{
	GpioCfg *i2s0_sclk = new_icelake_gpio(GPP_R0);

	const struct pad_config i2s_gpio_table[] = {
		/* I2S0_SCLK */	PAD_CFG_NF(GPP_R0, NONE, DEEP, NF2),
		/* I2S0_SFRM */	PAD_CFG_NF(GPP_R1, NONE, DEEP, NF2),
		/* I2S0_TXD */	PAD_CFG_NF(GPP_R2, NONE, DEEP, NF2),
		/* I2S0_RXD */	PAD_CFG_NF(GPP_R3, NONE, DEEP, NF2)
	};

	i2s0_sclk->configure(i2s0_sclk, i2s_gpio_table,
			ARRAY_SIZE(i2s_gpio_table));
}

static int dragonegg_finalize(struct CleanupFunc *cleanup, CleanupType type)
{
	/* Configure I2S audio gpios into NF2 mode */
	dragonegg_config_i2s_audio();

	return 0;
}

static int dragonegg_finalize_cleanup_install(void)
{
	static CleanupFunc dev =
	{
		&dragonegg_finalize,
		CleanupOnLegacy,
		NULL
	};

	list_insert_after(&dev.list_node, &cleanup_funcs);
	return 0;
}

static int board_setup(void)
{
	sysinfo_install_flags(new_icelake_gpio_input_from_coreboot);

	/* TPM */
	dragonegg_setup_tpm();

	/* Chrome EC (eSPI) */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, 0, NULL);
	register_vboot_ec(&cros_ec->vboot, 0);

	/* 32MB SPI Flash */
	flash_set_ops(&new_mem_mapped_flash(0xfe000000, 0x2000000)->ops);

	/* PCH Power */
	power_set_ops(&icelake_power_ops);

	/* eMMC */
	SdhciHost *emmc = new_pci_sdhci_host(PCH_DEV_EMMC),
			SDHCI_PLATFORM_SUPPORTS_HS400ES,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* SD Card */
	SdhciHost *sd = new_pci_sdhci_host(PCH_DEV_SDCARD, 1,
			EMMC_SD_CLOCK_MIN, SD_CLOCK_MAX);
	list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
			&removable_block_dev_controllers);

	/* NVMe */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCH_DEV_PCIE9);
	list_insert_after(&nvme->ctrlr.list_node,
			  &fixed_block_dev_controllers);

	/* Boot Beep Support */

	/* Use GPIOs to provide a BCLK+LRCLK to the codec */
	GpioCfg *boot_beep_bclk = new_icelake_gpio_output(GPP_R0, 0);
	GpioCfg *boot_beep_lrclk = new_icelake_gpio_output(GPP_R1, 0);

	/* Speaker amp is Maxim 98373 codec on I2C3 */
	DesignwareI2c *i2c3 = new_pci_designware_i2c(
						PCI_DEV(0, 0x15, 3),
						400000, 120);

	Max98373Codec *tone_generator =
		new_max98373_tone_generator(&i2c3->ops, 0x32, 48000,
					    &boot_beep_bclk->ops,
					    &boot_beep_lrclk->ops);
	sound_set_ops(&tone_generator->ops);

	dragonegg_finalize_cleanup_install();

	return 0;
}

INIT_FUNC(board_setup);

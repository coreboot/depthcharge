// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC
 *
 * Board file for Dedede.
 */

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/spi/intel_gspi.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/jasperlake.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/jasperlake.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/spi.h"

/*
 * Clock frequencies for the eMMC and SD ports are defined below. The minimum
 * frequency is the same for both interfaces, the firmware does not run any
 * interface faster than 52 MHz, but defines maximum eMMC frequency as 200 MHz
 * for proper divider settings.
 */
#define EMMC_SD_CLOCK_MIN       400000
#define EMMC_CLOCK_MAX          200000000
#define SD_CLOCK_MAX		52000000

static int cr50_irq_status(void)
{
	return jasperlake_get_gpe(GPE0_DW0_04); /* GPP_B4 */
}

static void dedede_setup_tpm(void)
{
	if (CONFIG(DRIVER_TPM_SPI)) {
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

static int board_setup(void)
{
	sysinfo_install_flags(new_jasperlake_gpio_input_from_coreboot);

	/* 32MB SPI Flash */
	flash_set_ops(&new_mem_mapped_flash(0xfe000000, 0x2000000)->ops);

	/* TPM */
	dedede_setup_tpm();

	/* Chrome EC (eSPI) */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);

	/* PCH Power */
	power_set_ops(&jasperlake_power_ops);

	/* eMMC */
	SdhciHost *emmc = new_pci_sdhci_host((PCH_DEV_EMMC),
			SDHCI_PLATFORM_SUPPORTS_HS400ES,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* SD Card */
	SdhciHost *sd = new_pci_sdhci_host(PCH_DEV_SDCARD, 1,
			EMMC_SD_CLOCK_MIN, SD_CLOCK_MAX);
	list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
			&removable_block_dev_controllers);

	return 0;
}

INIT_FUNC(board_setup);
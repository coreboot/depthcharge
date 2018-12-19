/*
 * Copyright 2018 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <arch/io.h>
#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "config.h"
#include "drivers/bus/spi/intel_gspi.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/cannonlake.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/spi.h"
#include "drivers/tpm/tpm.h"

/* Clock frequencies for the SD ports are defined below */
#define SD_CLOCK_MIN	400000
#define SD_CLOCK_MAX	52000000

static int cr50_irq_status(void)
{
	return cannonlake_get_gpe(GPE0_DW1_21);
}

static void hatch_setup_tpm(void)
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

static int board_setup(void)
{
	sysinfo_install_flags(NULL);

	/* TPM */
	hatch_setup_tpm();

	/* Chrome EC (eSPI) */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, 0, NULL);
	register_vboot_ec(&cros_ec->vboot, 0);

	/* 32MB SPI Flash */
	flash_set_ops(&new_mem_mapped_flash(0xfe000000, 0x2000000)->ops);

	/* Cannonlake PCH */
	power_set_ops(&cannonlake_power_ops);

	/* SATA AHCI */
	AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV(0, 0x17, 0));
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	/* NVMe SSD */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(0, 0x1d, 0));
	list_insert_after(&nvme->ctrlr.list_node,
				&fixed_block_dev_controllers);

	/* SD Card */
	SdhciHost *sd = new_pci_sdhci_host(PCI_DEV(0, 0x14, 5), 1,
			SD_CLOCK_MIN, SD_CLOCK_MAX);
	list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
			&removable_block_dev_controllers);

	return 0;
}

INIT_FUNC(board_setup);

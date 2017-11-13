/*
 * Copyright 2017 Google Inc.
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
 */

#include <arch/io.h>
#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>
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
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/cannonlake.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/blockdev.h"
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

static int cr50_irq_status(void)
{
	return cannonlake_get_gpe(GPE0_DW2_12);
}

static void meowth_setup_tpm(void)
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
	meowth_setup_tpm();

	/* Chrome EC (eSPI) */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, 0, NULL);
	register_vboot_ec(&cros_ec->vboot, 0);

	/* 16MB SPI Flash */
	flash_set_ops(&new_mem_mapped_flash(0xff000000, 0x1000000)->ops);

	/* PCH Power */
	power_set_ops(&cannonlake_power_ops);

	uintptr_t UsbMmioBase =
		pci_read_config32(PCI_DEV(0, 0x14, 0), PCI_BASE_ADDRESS_0);
	UsbMmioBase &= 0xFFFF0000; /* 32 bits only */
	UsbHostController *usb_host1 = new_usb_hc(XHCI, UsbMmioBase);
	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

	/* eMMC */
	SdhciHost *emmc = new_pci_sdhci_host(PCI_DEV(0, 0x1a, 0), 0,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);
	return 0;
}

INIT_FUNC(board_setup);

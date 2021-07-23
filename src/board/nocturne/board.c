/*
 * Copyright (C) 2018 Google Inc.
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
#include "boot/commandline.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/bus/spi/intel_gspi.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/ec/ps8751/ps8751.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/skylake.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/skylake.h"
#include "drivers/sound/max98373.h"
#include "drivers/sound/route.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/spi.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"

#ifdef PD_SYNC
#error "PD_SYNC configuration is incompatible with ec_tunnel!"
#endif /* PD_SYNC */

/*
 * Clock frequencies for the eMMC and SD ports are defined below. The minimum
 * frequency is the same for both interfaces, the firmware does not run any
 * interface faster than 52 MHz, but defines maximum eMMC frequency as 200 MHz
 * for proper divider settings.
 */
#define EMMC_CLOCK_MIN	400000
#define EMMC_CLOCK_MAX	200000000

static int cr50_irq_status(void)
{
	return skylake_get_gpe(GPE0_DW2_00);
}

static int board_setup(void)
{
	CrosECTunnelI2c *cros_ec_i2c_tunnel;

	sysinfo_install_flags(new_skylake_gpio_input_from_coreboot);

	/* SPI TPM */
	const IntelGspiSetupParams gspi0 = {
		.dev = PCI_DEV(0, 0x1e, 2),
		.cs_polarity = SPI_POLARITY_LOW,
		.clk_phase = SPI_CLOCK_PHASE_FIRST,
		.clk_polarity = SPI_POLARITY_LOW,
		.ref_clk_mhz = 120,
		.gspi_clk_mhz = 1,
	};
	tpm_set_ops(&new_tpm_spi(new_intel_gspi(&gspi0), cr50_irq_status)->ops);

	/* Chrome EC (eSPI) */
	CrosEcLpcBus *espi_ec = new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	GpioOps *ec_gpio = sysinfo_lookup_gpio("EC sync gpio", 1,
					new_skylake_gpio_input_from_coreboot);
	CrosEc *cros_ec = new_cros_ec(&espi_ec->ops, ec_gpio);
	register_vboot_ec(&cros_ec->vboot);

	/* EC I2C Tunnel for TCPCs */
	cros_ec_i2c_tunnel = new_cros_ec_tunnel_i2c(cros_ec, /* i2c bus */ 1);
	Ps8751 *ps8805_p0 = new_ps8805_a2(cros_ec_i2c_tunnel, /* ec pd# */ 0);
	register_vboot_auxfw(&ps8805_p0->fw_ops);

	cros_ec_i2c_tunnel = new_cros_ec_tunnel_i2c(cros_ec, /* i2c bus */ 2);
	Ps8751 *ps8805_p1 = new_ps8805_a2(cros_ec_i2c_tunnel, /* ec pd# */ 1);
	register_vboot_auxfw(&ps8805_p1->fw_ops);

	/* PCH Power */
	power_set_ops(&skylake_power_ops);

	/* eMMC */
	SdhciHost *emmc = new_pci_sdhci_host(PCI_DEV(0, 0x1e, 4),
			SDHCI_PLATFORM_NO_EMMC_HS200,
			EMMC_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* NVME SSD - Root port 9 */
	NvmeCtrlr *nvme1 = new_nvme_ctrlr(PCI_DEV(0, 0x1D, 0));
	list_insert_after(&nvme1->ctrlr.list_node,
			  &fixed_block_dev_controllers);

	/* Activate buffer to disconnect I2S from PCH and allow GPIO */
	GpioCfg *boot_beep_buffer_enable =
			new_skylake_gpio_output(GPP_F1, 1);
	gpio_set(&boot_beep_buffer_enable->ops, 0);

	/* Use GPIOs to provide a BCLK+LRCLK to the codec */
	GpioCfg *boot_beep_bclk = new_skylake_gpio_output(GPP_F0, 0);
	GpioCfg *boot_beep_lrclk = new_skylake_gpio_output(GPP_F2, 0);

	/* Speaker amp is Maxim 98373 codec on I2C4 */
	DesignwareI2c *i2c4 = new_pci_designware_i2c(
						PCI_DEV(0, 0x19, 2),
						400000, 120);

	Max98373Codec *tone_generator =
		new_max98373_tone_generator(&i2c4->ops, 0x32, 48000,
					    &boot_beep_bclk->ops,
					    &boot_beep_lrclk->ops);
	sound_set_ops(&tone_generator->ops);

	return 0;
}

INIT_FUNC(board_setup);

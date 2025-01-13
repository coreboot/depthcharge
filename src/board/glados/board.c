/*
 * Copyright 2015 Google LLC
 * Copyright (C) 2015 Intel Corporation
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

#include <pci.h>
#include <pci/pci.h>
#include <libpayload.h>
#include <sysinfo.h>
#include <vb2_api.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/gpio/skylake.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/sound/gpio_pdm.h"
#include "drivers/sound/route.h"
#include "drivers/sound/ssm4567.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/lpc.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"

/*
 * Clock frequencies for the eMMC and SD ports are defined below. The minimum
 * frequency is the same for both interfaces, the firmware does not run any
 * interface faster than 52 MHz, but defines maximum eMMC frequency as 200 MHz
 * for proper divider settings.
 */
#define EMMC_SD_CLOCK_MIN	400000
#define EMMC_CLOCK_MAX		200000000
#define SD_CLOCK_MAX		52000000

static int board_setup(void)
{
	sysinfo_install_flags(new_skylake_gpio_input_from_coreboot);

	/* MEC1322 Chrome EC */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_MEC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);

	/* SPI TPM memory mapped to act like LPC TPM */
	tpm_set_ops(&new_lpc_tpm((void *)(uintptr_t)0xfed40000)->ops);

	/* PCH Power */
	power_set_ops(&skylake_power_ops);

	/* eMMC */
	SdhciHost *emmc = new_pci_sdhci_host(PCI_DEV(0, 0x1e, 4),
			SDHCI_PLATFORM_NO_EMMC_HS200,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* SD Card (if present) */
	pcidev_t sd_pci_dev = PCI_DEV(0, 0x1e, 6);
	uint16_t sd_vendor_id = pci_read_config32(sd_pci_dev, REG_VENDOR_ID);
	if (sd_vendor_id == PCI_VENDOR_ID_INTEL) {
		SdhciHost *sd = new_pci_sdhci_host(sd_pci_dev, 1,
					   EMMC_SD_CLOCK_MIN, SD_CLOCK_MAX);
		list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
				  &removable_block_dev_controllers);
	}

	/* Speaker Amp Codec is on I2C4 */
	DesignwareI2c *i2c4 =
		new_pci_designware_i2c(PCI_DEV(0, 0x19, 2), 400000, 120);
	ssm4567Codec *speaker_amp_left =
		new_ssm4567_codec(&i2c4->ops, 0x34, SSM4567_MODE_PDM);

	/* Activate buffer to disconnect I2S from PCH and allow GPIO */
	GpioCfg *i2s2_buffer = new_skylake_gpio_output(GPP_D22, 0);
	gpio_set(&i2s2_buffer->ops, 1);

	/* Use GPIO to bit-bang PDM to the codec */
	GpioCfg *i2s2_sclk = new_skylake_gpio_output(GPP_F0, 0);
	GpioCfg *i2s2_txd  = new_skylake_gpio_output(GPP_F2, 0);
	GpioPdm *pdm = new_gpio_pdm(&i2s2_sclk->ops,	/* PDM Clock GPIO */
				    &i2s2_txd->ops,	/* PDM Data GPIO */
				    85000,		/* Clock Start */
				    16000,		/* Sample Rate */
				    2,			/* Channels */
				    1000);		/* Volume */

	/* Connect the Codec to the PDM source */
	SoundRoute *sound = new_sound_route(&pdm->ops);
	list_insert_after(&speaker_amp_left->component.list_node,
			  &sound->components);
	sound_set_ops(&sound->ops);

	return 0;
}

INIT_FUNC(board_setup);

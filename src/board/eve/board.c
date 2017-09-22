/*
 * Copyright (C) 2016 Google Inc.
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
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/skylake.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/skylake.h"
#include "drivers/sound/gpio_i2s.h"
#include "drivers/sound/max98927.h"
#include "drivers/sound/route.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/tpm.h"
#include "vboot/boot_policy.h"
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

static int cr50_irq_status(void)
{
	return skylake_get_gpe(GPE0_DW2_00);
}

static int board_setup(void)
{
	static const struct boot_policy policy[] = {
		{KERNEL_IMAGE_MULTIBOOT, CMD_LINE_SIGNER},
		{KERNEL_IMAGE_CROS, CMD_LINE_SIGNER},
	};
	sysinfo_install_flags(new_skylake_gpio_input_from_coreboot);

	if (IS_ENABLED(CONFIG_KERNEL_MULTIBOOT))
		set_boot_policy(policy, ARRAY_SIZE(policy));

	/* Chrome EC (eSPI) */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, 0, NULL);
	register_vboot_ec(&cros_ec->vboot, 0);

	/* 16MB SPI Flash */
	flash_set_ops(&new_mem_mapped_flash(0xff000000, 0x1000000)->ops);

	/* H1 TPM on I2C bus 1 @ 400KHz, controller core is 120MHz */
	DesignwareI2c *i2c1 = new_pci_designware_i2c(
		PCI_DEV(0, 0x15, 1), 400000, SKYLAKE_DW_I2C_MHZ);
	tpm_set_ops(&new_cr50_i2c(&i2c1->ops, 0x50,
				  &cr50_irq_status)->base.ops);

	/* PCH Power */
	power_set_ops(&skylake_power_ops);

	/* eMMC */
	SdhciHost *emmc = new_pci_sdhci_host(PCI_DEV(0, 0x1e, 4),
			SDHCI_PLATFORM_NO_EMMC_HS200,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	/* NVMe Root Port */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(0, 0x1c, 4));
	nvme_add_static_namespace(nvme, 1, 512, 0x3b9e12b0, "KUS040205M");
	nvme_add_static_namespace(nvme, 1, 512, 0x1dcf32b0, "KUS030205M");
	list_insert_after(&nvme->ctrlr.list_node, &fixed_block_dev_controllers);

	/* Backup NVMe Root Port in case WiFi device is missing */
	NvmeCtrlr *nvmeb = new_nvme_ctrlr(PCI_DEV(0, 0x1c, 0));
	nvme_add_static_namespace(nvmeb, 1, 512, 0x3b9e12b0, "KUS040205M");
	nvme_add_static_namespace(nvmeb, 1, 512, 0x1dcf32b0, "KUS030205M");
	list_insert_after(&nvmeb->ctrlr.list_node,
			  &fixed_block_dev_controllers);

	/* Speaker amp is Maxim 98927 codec on I2C4 */
	DesignwareI2c *i2c4 =
		new_pci_designware_i2c(PCI_DEV(0, 0x19, 2), 400000, 120);
	Max98927Codec *speaker_amp =
		new_max98927_codec(&i2c4->ops, 0x39, 16, 16000, 64);

	/* Activate buffer to disconnect I2S from PCH and allow GPIO */
	GpioCfg *i2s_buffer = new_skylake_gpio_output(GPP_D22, 1);
	gpio_set(&i2s_buffer->ops, 0);

	/* Use GPIO to provide PCM clock to the codec */
	GpioCfg *i2s2_bclk = new_skylake_gpio_output(GPP_F0, 0);
	GpioCfg *i2s2_sfrm = new_skylake_gpio_output(GPP_F1, 0);
	GpioCfg *i2s2_txd  = new_skylake_gpio_output(GPP_F2, 0);
	GpioI2s *i2s = new_gpio_i2s(
			&i2s2_bclk->ops,    /* I2S Bit Clock GPIO */
			&i2s2_sfrm->ops,    /* I2S Frame Sync GPIO */
			&i2s2_txd->ops,     /* I2S Data GPIO */
			16000,              /* Sample rate */
			2,                  /* Channels */
			0x1FFF);            /* Volume */

	/* Connect the speaker amp to the PCM clock source */
	SoundRoute *sound = new_sound_route(&i2s->ops);
	list_insert_after(&speaker_amp->component.list_node,
			  &sound->components);
	sound_set_ops(&sound->ops);

	return 0;
}

INIT_FUNC(board_setup);

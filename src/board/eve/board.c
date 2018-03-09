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
#include <endian.h>
#include <gbb_header.h>
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
#include "drivers/ec/anx3429/anx3429.h"
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
#include "vboot/stages.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"
#include "vboot/util/init_funcs.h"

/*
 * Clock frequencies for the eMMC and SD ports are defined below. The minimum
 * frequency is the same for both interfaces, the firmware does not run any
 * interface faster than 52 MHz, but defines maximum eMMC frequency as 200 MHz
 * for proper divider settings.
 */
#define EMMC_SD_CLOCK_MIN	400000
#define EMMC_CLOCK_MAX		200000000
#define SD_CLOCK_MAX		52000000

/*
 * Read device ID register from rt5514 codec to check audio health and hard
 * reset the system via cr50 if there are any issues found.  A byte of CMOS
 * is used to ensure the workaround is only attempted once and the system
 * does not get stuck in a reset loop.
 */
#define RT5514_I2C_SLAVE_ADDR	0x57
#define RT5514_DEVID_REG	0x18002ff4
#define RT5514_DEVID_VALID	0x10ec5514
#define CMOS_RESET_REG		0x64
#define CMOS_RESET_MAGIC	0xad

static int read_rt5514_id(DesignwareI2c *i2c, uint32_t *id)
{
	uint32_t device_id_reg = htobe32(RT5514_DEVID_REG);
	I2cSeg seg[2];

	seg[0].read = 0;
	seg[0].buf = (uint8_t *)&device_id_reg;
	seg[0].len = sizeof(device_id_reg);
	seg[0].chip = RT5514_I2C_SLAVE_ADDR;

	seg[1].read = 1;
	seg[1].buf = (uint8_t *)id;
	seg[1].len = sizeof(*id);
	seg[1].chip = RT5514_I2C_SLAVE_ADDR;

	return i2c->ops.transfer(&i2c->ops, seg, ARRAY_SIZE(seg));
}

static int board_check_audio(VbootInitFunc *init)
{
	DesignwareI2c *i2c = init->data;
	GoogleBinaryBlockHeader *gbb = cparams.gbb_data;
	uint8_t reset_reg = nvram_read(CMOS_RESET_REG);
	uint32_t device_id_valid = htobe32(RT5514_DEVID_VALID);
	uint32_t device_id = 0;
	const uint8_t cr50_reset[] = {
		0x80, 0x01,		/* TPM_ST_NO_SESSIONS */
		0x00, 0x00, 0x00, 0x0c,	/* commandSize */
		0x20, 0x00, 0x00, 0x00,	/* cr50 vendor command */
		0x00, 0x13		/* immediate reset command */
	};
	int ret;

	/* Skip in recovery mode */
	if (vboot_in_recovery())
		return 0;

	/* Skip if FAFT is enabled */
	if (gbb->flags & GBB_FLAG_FAFT_KEY_OVERIDE)
		return 0;

	ret = read_rt5514_id(i2c, &device_id);
	if (ret || device_id != device_id_valid) {
		/* Re-read once on failure */
		ret = read_rt5514_id(i2c, &device_id);
	}

	if (ret || device_id != device_id_valid) {
		printf("Audio is broken\n");
		/* Only reset if CMOS has not been set to magic value */
		if (reset_reg != CMOS_RESET_MAGIC) {
			printf("Reset system via cr50\n");
			nvram_write(CMOS_RESET_MAGIC, CMOS_RESET_REG);
			tpm_xmit(cr50_reset, ARRAY_SIZE(cr50_reset), NULL, 0);
		} else {
			printf("Audio recovery failed\n");
		}
	} else if (reset_reg == CMOS_RESET_MAGIC) {
		/* Audio is OK, clear CMOS register to reset workaround */
		printf("Audio is working again\n");
	}

	/* Clear CMOS register to reset workaround */
	if (reset_reg != 0)
		nvram_write(0, CMOS_RESET_REG);

	return 0;
}

VbootInitFunc audio_init_func = {
	.init = &board_check_audio
};

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

	/* Downstream EC devices */
	CrosECTunnelI2c *cros_ec_i2c0 = new_cros_ec_tunnel_i2c(cros_ec, 0);
	Anx3429 *anx3429_0 = new_anx3429(cros_ec_i2c0, 0);
	register_vboot_aux_fw(&anx3429_0->fw_ops);

	CrosECTunnelI2c *cros_ec_i2c1 = new_cros_ec_tunnel_i2c(cros_ec, 1);
	Anx3429 *anx3429_1 = new_anx3429(cros_ec_i2c1, 1);
	register_vboot_aux_fw(&anx3429_1->fw_ops);

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

	/* Check audio health before vboot takes control */
	audio_init_func.data = i2c4;
	list_insert_after(&audio_init_func.list_node, &vboot_init_funcs);

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

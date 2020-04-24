/*
 * Copyright 2019 Google Inc.
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

#include <arch/msr.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/ec/anx3429/anx3429.h"
#include "drivers/ec/ps8751/ps8751.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/kern.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/fch.h"
#include "drivers/soc/picasso.h"
#include "drivers/sound/gpio_i2s.h"
#include "drivers/sound/max98357a.h"
#include "drivers/sound/route.h"
#include "drivers/sound/rt5682.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/bayhub.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/video/display.h"
#include "drivers/bus/usb/usb.h"
#include "pci.h"
#include "vboot/util/flag.h"

/* SD Controllers */
#define BH720_PCI_VID		0x1217
#define BH720_PCI_DID		0x8621

#define GENESYS_PCI_VID		0x17a0
#define GL9755S_PCI_DID		0x9755
#define GL9750S_PCI_DID		TODO

/* I2S Beep GPIOs */
#define I2S_LRCLK_GPIO		8
#define I2S_DATA_GPIO		135
#define EN_SPKR			91

/* Clock frequencies */
#define MCLK			4800000
#define LRCLK			8000

/* SPI Flash */
#define FLASH_SIZE		0x1000000	/* 16MB */
#define FLASH_START		( 0xffffffff - FLASH_SIZE + 1 )

/* Core boost register */
#define HW_CONFIG_REG 0xc0010015
#define   HW_CONFIG_CPBDIS (1 << 25)

/* cr50's interrupt is attached to GPIO_3 */
#define CR50_INT		3

/* eDP backlight */
#define GPIO_BACKLIGHT		85

static int cr50_irq_status(void)
{
	static KernGpio *tpm_gpio;

	if (!tpm_gpio)
		tpm_gpio = new_kern_fch_gpio_latched(CR50_INT);

	return gpio_get(&tpm_gpio->ops);
}

static int zork_backlight_update(DisplayOps *me, uint8_t enable)
{
	/* Backlight signal is active low */
	static KernGpio *backlight;

	if (!backlight)
		backlight = new_kern_fch_gpio_output(GPIO_BACKLIGHT, !enable);
	else
		backlight->ops.set(&backlight->ops, !enable);

	return 0;
}

static DisplayOps zork_display_ops = {
	.backlight_update = &zork_backlight_update,
};

static int (*gpio_i2s_play)(struct SoundOps *me, uint32_t msec,
		uint32_t frequency);

static int amd_gpio_i2s_play(struct SoundOps *me, uint32_t msec,
		uint32_t frequency)
{
	int ret;
	uint64_t cur;

	cur = _rdmsr(HW_CONFIG_REG);

	/* Disable Core Boost while bit-banging I2S */
	_wrmsr(HW_CONFIG_REG, cur | HW_CONFIG_CPBDIS);

	ret = gpio_i2s_play(me, msec, frequency);

	/* Restore previous Core Boost setting */
	_wrmsr(HW_CONFIG_REG, cur);

	return ret;
}

static int is_dalboz(void)
{
	const char * const dalboz_str = "Dalboz";
	struct cb_mainboard *mainboard = lib_sysinfo.mainboard;

	return strncasecmp(cb_mb_part_string(mainboard),
			   dalboz_str, strlen(dalboz_str)) == 0;
}

static void audio_setup(CrosEc *cros_ec)
{
	CrosECTunnelI2c *cros_ec_i2c_tunnel;

	if (!is_dalboz())
		cros_ec_i2c_tunnel = new_cros_ec_tunnel_i2c(cros_ec,
							    /* i2c bus */ 8);
	else
		cros_ec_i2c_tunnel = new_cros_ec_tunnel_i2c(cros_ec,
							    /* i2c bus */ 5);

	rt5682Codec *rt5682 = new_rt5682_codec(&cros_ec_i2c_tunnel->ops, 0x1a);
	if (rt5682_enable(rt5682)) {
		printf("%s: error in enabling codec\n", __func__);
		return;
	}
	if (rt5682_set_clock(rt5682, MCLK, LRCLK)) {
		printf("%s: error in setting up clocks\n", __func__);
		return;
	}

	KernGpio *i2s_lrclk = new_kern_fch_gpio_input(I2S_LRCLK_GPIO);
	KernGpio *i2s2_data = new_kern_fch_gpio_output(I2S_DATA_GPIO, 0);
	GpioI2s *i2s = new_gpio_i2s(
			NULL,			/* Use RT5682 to give clks */
			&i2s_lrclk->ops,	/* I2S Frame Sync GPIO */
			&i2s2_data->ops,	/* I2S Data GPIO */
			16000,			/* Sample rate */
			2,			/* Channels */
			0x1FFF);		/* Volume */
	SoundRoute *sound_route = new_sound_route(&i2s->ops);

	/*
	 * Override gpio_i2s play() op with our own that disbles CPU boost
	 * before GPIO bit-banging I2S.
	 */
	gpio_i2s_play = i2s->ops.play;
	i2s->ops.play = amd_gpio_i2s_play;

	KernGpio *spk_pa_en = new_kern_fch_gpio_output(EN_SPKR, 1);

	/* Codec for Grunt, should work with Zork */
	max98357aCodec *speaker_amp = new_max98357a_codec(&spk_pa_en->ops);

	list_insert_after(&speaker_amp->component.list_node,
			  &sound_route->components);

	sound_set_ops(&sound_route->ops);
}

static int board_setup(void)
{
	sysinfo_install_flags(NULL);
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);

	flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());

	flash_set_ops(&new_mem_mapped_flash(FLASH_START, FLASH_SIZE)->ops);

	audio_setup(cros_ec);

	SdhciHost *sd = NULL;
	pcidev_t pci_dev;
	if (pci_find_device(BH720_PCI_VID, BH720_PCI_DID, &pci_dev)) {
		sd = new_bayhub_sdhci_host(pci_dev,
				SDHCI_PLATFORM_REMOVABLE,
				0, 0);
	} else if (pci_find_device(GENESYS_PCI_VID, GL9755S_PCI_DID,
				   &pci_dev)) {
		sd = new_pci_sdhci_host(pci_dev, SDHCI_PLATFORM_REMOVABLE, 0,
					0);
	}

	if (sd) {
		sd->name = "SD";
		list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
				&removable_block_dev_controllers);
	} else {
		printf("Failed to find SD card reader\n");
	}

	SdhciHost *emmc = new_mem_sdhci_host(
		EMMCCFG,
		/* Can't enable HS200 or HS400 until tuning is fixed */
		SDHCI_PLATFORM_NO_EMMC_HS200 | SDHCI_PLATFORM_EMMC_1V8_POWER, 0,
		0, 0);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	/* PCI Bridge for NVMe */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(0, 0x01, 0x07));
	list_insert_after(&nvme->ctrlr.list_node,
				&fixed_block_dev_controllers);

	NvmeCtrlr *nvme_dal = new_nvme_ctrlr(PCI_DEV(0, 0x01, 0x02));
	list_insert_after(&nvme_dal->ctrlr.list_node,
				&fixed_block_dev_controllers);

	/* Set up h1 on I2C3 */
	DesignwareI2c *i2c_h1 = new_designware_i2c(
		AP_I2C3_ADDR, 400000, AP_I2C_CLK_MHZ);
	tpm_set_ops(&new_cr50_i2c(&i2c_h1->ops, 0x50,
				  &cr50_irq_status)->base.ops);

	power_set_ops(&kern_power_ops);
	display_set_ops(&zork_display_ops);

	return 0;
}

INIT_FUNC(board_setup);

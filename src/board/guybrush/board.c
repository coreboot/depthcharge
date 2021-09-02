// SPDX-License-Identifier: GPL-2.0
/* Copyright 2021 Google LLC.  */

#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/ec/anx3429/anx3429.h"
#include "drivers/ec/ps8751/ps8751.h"
#include "drivers/gpio/gpio.h"
#include "drivers/sound/gpio_amp.h"
#include "drivers/sound/gpio_i2s.h"
#include "drivers/gpio/kern.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/fch.h"
#include "drivers/soc/cezanne.h"
#include "drivers/sound/amd_i2s_support.h"
#include "drivers/sound/rt1019.h"
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
#define GL9750S_PCI_DID		0x9750

/* Clock frequencies */
#define MCLK			4800000
#define LRCLK			8000

/* cr50 / Ti50 interrupt is attached to GPIO_3 */
#define CR50_INT		3

/* eDP backlight */
#define GPIO_BACKLIGHT		129

/* Audio Configuration */
#define AUDIO_I2C_MMIO_ADDR	0xfedc4000
#define AUDIO_I2C_SPEED		400000
#define I2C_DESIGNWARE_CLK_MHZ	150

/* I2S Beep GPIOs */
#define I2S_BCLK_GPIO		88
#define I2S_LRCLK_GPIO		75
#define I2S_DATA_GPIO		87
#define EN_SPKR			31

/* FW_CONFIG for beep banging */
#define FW_CONFIG_BIT_BANGING (1 << 9)

static int cr50_irq_status(void)
{
	static KernGpio *tpm_gpio;

	if (!tpm_gpio)
		tpm_gpio = new_kern_fch_gpio_latched(CR50_INT);

	return gpio_get(&tpm_gpio->ops);
}

static int guybrush_backlight_update(DisplayOps *me, uint8_t enable)
{
	/* Backlight signal is active low */
	static KernGpio *backlight;

	if (!backlight)
		backlight = new_kern_fch_gpio_output(GPIO_BACKLIGHT, !enable);
	else
		backlight->ops.set(&backlight->ops, !enable);

	return 0;
}

static void setup_rt1019_amp(void)
{
	DesignwareI2c *i2c = new_designware_i2c((uintptr_t)AUDIO_I2C_MMIO_ADDR,
				       AUDIO_I2C_SPEED, I2C_DESIGNWARE_CLK_MHZ);
	rt1019Codec *speaker_amp = new_rt1019_codec(&i2c->ops,
						AUD_RT1019_DEVICE_ADDR_R);
	SoundRoute *sound_route = new_sound_route(&speaker_amp->ops);

	list_insert_after(&speaker_amp->component.list_node,
						&sound_route->components);
	sound_set_ops(&sound_route->ops);
}

static DisplayOps guybrush_display_ops = {
	.backlight_update = &guybrush_backlight_update,
};

static void setup_bit_banging(void)
{
	KernGpio *i2s_bclk = new_kern_fch_gpio_input(I2S_BCLK_GPIO);
	KernGpio *i2s_lrclk = new_kern_fch_gpio_input(I2S_LRCLK_GPIO);
	KernGpio *i2s_data = new_kern_fch_gpio_output(I2S_DATA_GPIO, 0);
	KernGpio *spk_pa_en = new_kern_fch_gpio_output(EN_SPKR, 1);

	DesignwareI2c *i2c = new_designware_i2c((uintptr_t)AUDIO_I2C_MMIO_ADDR,
				       AUDIO_I2C_SPEED, I2C_DESIGNWARE_CLK_MHZ);

	GpioI2s *i2s = new_gpio_i2s(
			&i2s_bclk->ops,		/* Use RT5682 to give clks */
			&i2s_lrclk->ops,	/* I2S Frame Sync GPIO */
			&i2s_data->ops,		/* I2S Data GPIO */
			8000,			/* Sample rate */
			2,			/* Channels */
			0x1FFF,			/* Volume */
			1);			/* BCLK sync */

	SoundRoute *sound_route = new_sound_route(&i2s->ops);

	rt5682Codec *rt5682 = new_rt5682_codec(&i2c->ops, 0x1a, MCLK, LRCLK);

	list_insert_after(&rt5682->component.list_node,
			  &sound_route->components);

	GpioAmpCodec *enable_spk = new_gpio_amp_codec(&spk_pa_en->ops);
	list_insert_after(&enable_spk->component.list_node,
			  &sound_route->components);

	amdI2sSupport *amdI2s = new_amd_i2s_support();
	list_insert_after(&amdI2s->component.list_node,
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

	if (lib_sysinfo.fw_config & FW_CONFIG_BIT_BANGING)
		setup_bit_banging();
	else
		setup_rt1019_amp();

	SdhciHost *sd = NULL;
	pcidev_t pci_dev;
	if (pci_find_device(BH720_PCI_VID, BH720_PCI_DID, &pci_dev)) {
		sd = new_bayhub_sdhci_host(pci_dev,
				SDHCI_PLATFORM_REMOVABLE,
				0, 0);
	} else if (pci_find_device(GENESYS_PCI_VID, GL9755S_PCI_DID,
				&pci_dev)) {
		sd = new_pci_sdhci_host(pci_dev,
				SDHCI_PLATFORM_REMOVABLE,
				0, 0);
	} else if (pci_find_device(GENESYS_PCI_VID, GL9750S_PCI_DID,
				&pci_dev)) {
		sd = new_pci_sdhci_host(pci_dev,
				SDHCI_PLATFORM_REMOVABLE,
				0, 0);
	}

	if (sd) {
		sd->name = "SD";
		list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
				&removable_block_dev_controllers);
	} else {
		printf("Failed to find SD card reader\n");
	}

	/* PCI Bridge for NVMe */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(0, 0x02, 0x04));
	list_insert_after(&nvme->ctrlr.list_node,
				&fixed_block_dev_controllers);

	/* Set up H1 / Dauntless on I2C3 */
	DesignwareI2c *i2c_h1 = new_designware_i2c(
		AP_I2C3_ADDR, 400000, AP_I2C_CLK_MHZ);
	tpm_set_ops(&new_cr50_i2c(&i2c_h1->ops, 0x50,
				  &cr50_irq_status)->base.ops);

	power_set_ops(&kern_power_ops);
	display_set_ops(&guybrush_display_ops);

	return 0;
}

INIT_FUNC(board_setup);

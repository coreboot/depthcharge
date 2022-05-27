// SPDX-License-Identifier: GPL-2.0
/* Copyright 2021 Google LLC.  */

#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "board/guybrush/include/variant.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/ec/anx3429/anx3429.h"
#include "drivers/ec/ps8751/ps8751.h"
#include "drivers/gpio/gpio.h"
#include "drivers/sound/gpio_amp.h"
#include "drivers/gpio/kern.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/fch.h"
#include "drivers/soc/cezanne.h"
#include "drivers/sound/amd_acp.h"
#include "drivers/sound/rt1019.h"
#include "drivers/sound/rt5682s.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/video/display.h"
#include "drivers/bus/usb/usb.h"
#include "pci.h"
#include "vboot/util/flag.h"

/* Clock frequencies */
#define MCLK			48000000
#define LRCLK			8000

/* eDP backlight */
#define GPIO_BACKLIGHT		129

/* Audio Configuration */
#define AUDIO_I2C_MMIO_ADDR	0xfedc4000
#define AUDIO_I2C_SPEED		400000
#define I2C_DESIGNWARE_CLK_MHZ	150

static unsigned int cr50_int_gpio = CR50_INT_85;

__weak const struct storage_config *variant_get_storage_configs(size_t *count)
{
	static const struct storage_config storage_configs[] = {
		{ .media = STORAGE_NVME, .pci_dev = PCI_DEV(0, 0x02, 0x04)},
		{ .media = STORAGE_SDHCI, .pci_dev = PCI_DEV(0, 0x02, 0x02)},
	};

	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

__weak unsigned int variant_get_cr50_irq_gpio(void)
{
	return CR50_INT_85;
}

__weak SoundRouteComponent *variant_get_audio_codec(I2cOps *i2c, uint8_t chip,
						    uint32_t mclk,
						    uint32_t lrclk)
{
	rt5682sCodec *rt5682s = new_rt5682s_codec(i2c, chip, mclk, lrclk);

	return &rt5682s->component;
}

__weak enum audio_amp variant_get_audio_amp(void)
{
	return AUDIO_AMP_RT1019;
}

__weak unsigned int variant_get_en_spkr_gpio(void)
{
	return EN_SPKR;
}

static int cr50_irq_status(void)
{
	static KernGpio *tpm_gpio;

	if (!tpm_gpio)
		tpm_gpio = new_kern_fch_gpio_latched(cr50_int_gpio);

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

static DisplayOps guybrush_display_ops = {
	.backlight_update = &guybrush_backlight_update,
};

static void setup_amd_acp_i2s(pcidev_t acp_pci_dev)
{
	AmdAcp *acp = new_amd_acp(acp_pci_dev, ACP_OUTPUT_PLAYBACK, LRCLK,
				  0x1FFF /* Volume */);

	SoundRoute *sound_route = new_sound_route(&acp->sound_ops);


	DesignwareI2c *i2c = new_designware_i2c((uintptr_t)AUDIO_I2C_MMIO_ADDR,
				       AUDIO_I2C_SPEED, I2C_DESIGNWARE_CLK_MHZ);

	SoundRouteComponent *codec =
		variant_get_audio_codec(&i2c->ops, 0x1a, MCLK, LRCLK);

	KernGpio *en_spkr_gpio =
		new_kern_fch_gpio_output(variant_get_en_spkr_gpio(), 0);

	/* Note: component order matters here. The first thing inserted is the
	 * last thing enabled. */

	/* If a warm reboot was performed, we need to clear out any settings the
	 * OS has made to the RT1019. */
	if (variant_get_audio_amp() == AUDIO_AMP_RT1019) {
		rt1019Codec *speaker_amp =
			new_rt1019_codec(&i2c->ops, AUD_RT1019_DEVICE_ADDR_R);

		list_insert_after(&speaker_amp->component.list_node,
				  &sound_route->components);

		GpioAmpCodec *en_spkr = new_gpio_amp_codec(&en_spkr_gpio->ops);

		/* We need to enable the speaker amp before we can send I2C
		 * transactions. */
		list_insert_after(&en_spkr->component.list_node,
				  &sound_route->components);
	} else {
		/* Give the MAX98360A time to sample the BCLK */
		GpioAmpCodec *en_spkr =
			new_gpio_amp_codec_with_delay(&en_spkr_gpio->ops, 1000);

		/* Enable the speaker amp last since we have to delay to sample
		 * the clocks. */
		list_insert_after(&en_spkr->component.list_node,
				  &sound_route->components);
	}

	/* Enable the ACP so it can start sampling the clocks. */
	list_insert_after(&acp->component.list_node, &sound_route->components);

	/*
	 * We want the codec to initialize first so it can start generating the
	 * BCLK/LRCLK.
	 */
	list_insert_after(&codec->list_node, &sound_route->components);

	sound_set_ops(&sound_route->ops);
}

static int board_setup(void)
{
	sysinfo_install_flags(NULL);
	cr50_int_gpio = variant_get_cr50_irq_gpio();
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);

	flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());

	pcidev_t acp_pci_dev;
	if (pci_find_device(AMD_PCI_VID, AMD_FAM17H_ACP_PCI_DID, &acp_pci_dev))
		setup_amd_acp_i2s(acp_pci_dev);

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

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
#include "drivers/tpm/cr50_i2c.h"
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

/* I2S Beep GPIOs */
#define I2S_BCLK_GPIO		88
#define I2S_LRCLK_GPIO		75
#define I2S_DATA_GPIO		87
#define EN_SPKR			31

/* FW_CONFIG for beep banging */
#define FW_CONFIG_BIT_BANGING (1 << 9)

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

static void setup_ec_in_rw_gpio(void)
{
	static const char * const mb = "Guybrush";
	struct cb_mainboard *mainboard = phys_to_virt(lib_sysinfo.cb_mainboard);

	/*
	 * Board versions 1 & 2 support H1 DB, but the EC_IN_RW signal is not
	 * routed. So add a dummy EC_IN_RW GPIO to emulate EC is trusted.
	 */
	if (!strcmp(cb_mb_part_string(mainboard), mb) &&
	    (lib_sysinfo.board_id == UNDEFINED_STRAPPING_ID ||
	     lib_sysinfo.board_id < 3))
		flag_replace(FLAG_ECINRW, new_gpio_low());
}

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
	cr50_int_gpio = variant_get_cr50_irq_gpio();
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);

	flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());
	setup_ec_in_rw_gpio();

	if (lib_sysinfo.fw_config & FW_CONFIG_BIT_BANGING)
		setup_bit_banging();
	else
		setup_rt1019_amp();

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

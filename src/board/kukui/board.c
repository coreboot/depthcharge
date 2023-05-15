/*
 * Copyright 2018 Google LLC
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

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "drivers/bus/i2c/mtk_i2c.h"
#include "drivers/bus/i2s/mtk_v1.h"
#include "drivers/bus/spi/mtk.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/mtk_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/gpio_amp.h"
#include "drivers/sound/rt1015.h"
#include "drivers/sound/rt1015p.h"
#include "drivers/storage/mtk_mmc.h"
#include "drivers/tpm/google/spi.h"
#include "vboot/util/flag.h"

#include "drivers/video/display.h"
#include "drivers/video/mtk_ddp.h"

/* Map schematics net names to SoC ball names. */
#define GPIO_DISP_PWM PAD_DISP_PWM
#define GPIO_EN_LCD_BL PAD_PERIPHERAL_EN13
#define GPIO_SPI0_CSB PAD_SPI_CSB
#define GPIO_SPI2_CSB PAD_EINT0
#define GPIO_AP_SPI_FLASH_CS_L PAD_SPI1_CSB

static SoundRouteComponent *get_speaker_amp(int *early_init)
{
	GpioOps *spk_en = sysinfo_lookup_gpio("speaker enable", 1,
					      new_mtk_gpio_output);
	*early_init = 0;

	if (spk_en) {
		/* MAX98357A, or a GPIO AMP. */
		GpioAmpCodec *codec = new_gpio_amp_codec(spk_en);
		return &codec->component;
	}

	spk_en = sysinfo_lookup_gpio("rt1015p sdb", 1, new_mtk_gpio_output);
	if (spk_en) {
		/* RT1015Q in auto mode (rt1015p). */
		rt1015pCodec *codec = new_rt1015p_codec(spk_en);
		*early_init = 1;
		return &codec->component;
	}

	/*
	 * RT1015 in I2C mode.
	 *
	 * RT1015 is dual channel and AUD_RT1015_DEVICE_ADDR is only
	 * left (0x28) but that is fine for firmware to beep.
	 */
	MTKI2c *i2c6 = new_mtk_i2c(0x11005000, 0x11000600, I2C_APDMA_NOASYNC);
	rt1015Codec *codec = new_rt1015_codec(&i2c6->ops,
					      AUD_RT1015_DEVICE_ADDR, 0);
	return &codec->component;
}

static void sound_setup(void)
{
	int early_init = 0;

	MtkI2s *i2s2 = new_mtk_i2s(0x11220000, 2, 48000, AFE_I2S2_I2S3);
	I2sSource *i2s_source = new_i2s_source(&i2s2->ops, 48000, 2, 8000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);

	SoundRouteComponent *speaker_amp = get_speaker_amp(&early_init);
	list_insert_after(&speaker_amp->list_node, &sound_route->components);
	list_insert_after(&i2s2->component.list_node,
			  &sound_route->components);

	sound_set_ops(&sound_route->ops);

	/* If we know there will be display (and beep), do early init. */
	if (display_init_required() && early_init)
		rt1015p_early_init(&speaker_amp->ops, sound_route);
}

static int gsc_irq_status(void)
{
	static GpioOps *tpm_int;

	if (!tpm_int)
		tpm_int = sysinfo_lookup_gpio("TPM interrupt", 1,
					      new_mtk_eint);
	assert(tpm_int);
	return gpio_get(tpm_int);
}

int kukui_backlight_update(DisplayOps *me, uint8_t enable)
{
	static GpioOps *disp_pwm0, *backlight_en;

	if (!backlight_en) {
		disp_pwm0 = new_mtk_gpio_output(GPIO_DISP_PWM);
		backlight_en = new_mtk_gpio_output(GPIO_EN_LCD_BL);
	}

	/* Enforce enable to be either 0 or 1. */
	enable = !!enable;

	gpio_set(disp_pwm0, enable);
	gpio_set(backlight_en, enable);
	return 0;
}

static int board_setup(void)
{
	sysinfo_install_flags(new_mtk_gpio_input);
	flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	if (!CONFIG(DETACHABLE))
		flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());

	power_set_ops(&psci_power_ops);

	GpioOps *spi0_cs = new_gpio_not(new_mtk_gpio_output(GPIO_SPI0_CSB));
	MtkSpi *spi0 = new_mtk_spi(0x1100A000, spi0_cs);
	tpm_set_ops(&new_tpm_spi(&spi0->ops, gsc_irq_status)->ops);

	GpioOps *spi2_cs = new_gpio_not(new_mtk_gpio_output(GPIO_SPI2_CSB));
	MtkSpi *spi2 = new_mtk_spi(0x11012000, spi2_cs);
	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spi2->ops);
	GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					      new_mtk_gpio_input);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_spi_bus->ops, ec_int);
	register_vboot_ec(&cros_ec->vboot);

	GpioOps *spi1_cs = new_gpio_not(
			new_mtk_gpio_output(GPIO_AP_SPI_FLASH_CS_L));
	MtkSpi *spi1 = new_mtk_spi(0x11010000, spi1_cs);
	flash_set_ops(&new_spi_flash(&spi1->ops)->ops);

	MtkMmcTuneReg emmc_tune_reg = {
		.msdc_iocon = 0x1 << 8,
		.pad_tune = 0x10 << 16
	};
	MtkMmcHost *emmc = new_mtk_mmc_host(
		0x11230000, 0x11f50000, 400 * MHz, 50 * MHz, emmc_tune_reg, 8, 0, NULL,
		MTK_MMC_V2);

	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	UsbHostController *usb_host = new_usb_hc(XHCI, 0x11200000);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	sound_setup();

	/* Set display ops */
	if (display_init_required())
		display_set_ops(new_mtk_display(kukui_backlight_update,
						0x14008000, 2));
	return 0;
}

INIT_FUNC(board_setup);

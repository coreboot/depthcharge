// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2021 Google Inc.
 *
 * Board file for Cherry.
 */

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/late_init_funcs.h"
#include "drivers/bus/i2c/mtk_i2c.h"
#include "drivers/bus/i2s/mtk_v2.h"
#include "drivers/bus/spi/mtk.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/spi.h"
#include "drivers/flash/mtk_snfc.h"
#include "drivers/gpio/mtk_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98390.h"
#include "drivers/sound/rt1011.h"
#include "drivers/sound/rt1019b.h"
#include "drivers/storage/mtk_mmc.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/video/display.h"
#include "drivers/video/mtk_ddp.h"
#include "vboot/util/flag.h"

#define GPIO_SD_CD_ODL		PAD_I2SO1_D1
#define GPIO_XHCI_DONE		PAD_DGI_D4
#define GPIO_AP_EDP_BKLTEN	PAD_DGI_D5
#define GPIO_BL_PWM_1V8		PAD_DISP_PWM0
#define GPIO_AP_SPI_EC_CS_L	PAD_SPIM0_CSB

static void setup_rt1011(GpioOps *spk_en, GpioOps *spk_rst)
{
	MTKI2c *i2c2 = new_mtk_i2c(0x11E02000, 0x10220380, I2C_APDMA_ASYNC);
	rt1011Codec *rt1011 = new_rt1011_codec(&i2c2->ops,
					       AUD_RT1011_DEVICE_ADDR);

	SoundRoute *sound_route = new_sound_route(&rt1011->ops);

	gpio_set(spk_en, 1);
	gpio_set(spk_rst, 1);

	list_insert_after(&rt1011->component.list_node,
			  &sound_route->components);
	sound_set_ops(&sound_route->ops);
}

static void setup_max98390(GpioOps *spk_en)
{
	/*
	 * Currently MAX98390 only supports 24-bit word length and
	 * 16-bit bit length. Due to hardware limitation of MT8195 I2S,
	 * 32KHz should be used.
	 */
	MtkI2s *i2s2 = new_mtk_i2s(0x10890000, 2, 32 * KHz,
				   24, 16, AFE_I2S_I1O2);
	I2sSource *i2s_source = new_i2s_source(&i2s2->ops, 32 * KHz, 2, 8000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	MTKI2c *i2c2 = new_mtk_i2c(0x11E02000, 0x10220380, I2C_APDMA_ASYNC);
	Max98390Codec *speaker_r = new_max98390_codec(&i2c2->ops, 0x38);
	Max98390Codec *speaker_l = new_max98390_codec(&i2c2->ops, 0x39);

	gpio_set(spk_en, 1);

	/* Delay 1ms for I2C ready */
	mdelay(1);

	list_insert_after(&speaker_l->component.list_node,
			  &sound_route->components);
	list_insert_after(&speaker_r->component.list_node,
			  &sound_route->components);

	sound_set_ops(&sound_route->ops);
}

static void sound_setup(void)
{
	GpioOps *speaker_en = sysinfo_lookup_gpio("speaker enable", 1,
						  new_mtk_gpio_output);

	GpioOps *beep_en = sysinfo_lookup_gpio("beep enable", 1,
					       new_mtk_gpio_output);

	GpioOps *rt1011_rst = sysinfo_lookup_gpio("rt1011 reset", 1,
						  new_mtk_gpio_output);

	if (!speaker_en) {
		printf("no speaker gpio\n");
		return;
	}

	if (beep_en) {
		rt1019bCodec *rt1019 = new_rt1019b_codec(speaker_en, beep_en);
		sound_set_ops(&rt1019->ops);
	} else if (rt1011_rst) {
		setup_rt1011(speaker_en, rt1011_rst);
	} else {
		setup_max98390(speaker_en);
	}
}

static int cr50_irq_status(void)
{
	static GpioOps *cr50_irq;

	if (!cr50_irq)
		cr50_irq = sysinfo_lookup_gpio("TPM interrupt", 1,
						new_mtk_eint);

	return cr50_irq->get(cr50_irq);
}

int board_backlight_update(DisplayOps *me, uint8_t enable)
{
	static GpioOps *disp_pwm0, *backlight_en;

	if (!backlight_en) {
		disp_pwm0 = new_mtk_gpio_output(GPIO_BL_PWM_1V8);
		backlight_en = new_mtk_gpio_output(GPIO_AP_EDP_BKLTEN);
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

	MTKI2c *i2c3 = new_mtk_i2c(0x11E03000, 0x10220480, I2C_APDMA_ASYNC);
	tpm_set_ops(&new_cr50_i2c(&i2c3->ops, 0x50,
				  &cr50_irq_status)->base.ops);

	GpioOps *spi0_cs = new_gpio_not(
		new_mtk_gpio_output(GPIO_AP_SPI_EC_CS_L));
	MtkSpi *spi0 = new_mtk_spi(0x1100A000, spi0_cs);
	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spi0->ops);
	GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					      new_mtk_gpio_input);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_spi_bus->ops, ec_int);
	register_vboot_ec(&cros_ec->vboot);

	/* Setup NOR flash ops */
	MtkNorFlash *nor_flash = new_mtk_nor_flash(0x1132C000);
	flash_set_ops(&nor_flash->ops);

	MtkMmcTuneReg emmc_tune_reg = {
		.msdc_iocon = 0x1 << 8,
		.pad_tune = 0x10 << 16
	};
	MtkMmcHost *emmc = new_mtk_mmc_host(
		0x11230000, 0x11f50000, 400 * MHz, 200 * MHz, emmc_tune_reg, 8, 0, NULL,
		MTK_MMC_V2);

	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	MtkMmcTuneReg sd_card_tune_reg = {
		.msdc_iocon = 0x0,
		.pad_tune = 0x0
	};
	GpioOps *card_detect_ops = new_gpio_not(
		new_mtk_gpio_input(GPIO_SD_CD_ODL));
	MtkMmcHost *sd_card = new_mtk_mmc_host(
		0x11240000, 0x11c70000, 200 * MHz, 25 * MHz, sd_card_tune_reg, 4, 1,
		card_detect_ops, MTK_MMC_V2);

	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	UsbHostController *usb_host = new_usb_hc(XHCI, 0x11200000);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	sound_setup();

	/* Set display ops */
	if (display_init_required())
		display_set_ops(new_mtk_display(board_backlight_update,
						0x1C000000, 2));
	else
		printf("[%s] no display_init_required()!\n", __func__);

	return 0;
}

INIT_FUNC(board_setup);

static int enable_usb_vbus(struct LateInitFunc *init)
{
	/*
	 * To avoid USB detection issue (b/187149602), assert GPIO
	 * AP_XHCI_INIT_DONE to notify EC to enable USB VBUS when xHCI is
	 * initialized.
	 */
	GpioOps *pdn = new_mtk_gpio_output(GPIO_XHCI_DONE);
	gpio_set(pdn, 1);
	return 0;
}

LATE_INIT_FUNC(enable_usb_vbus);

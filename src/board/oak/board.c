/*
 * Copyright 2013 Google Inc.
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
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/bus/i2c/mtk_i2c.h"
#include "drivers/bus/i2s/mtk.h"
#include "drivers/bus/spi/mtk.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/anx7688/anx7688.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/mtk_gpio.h"
#include "drivers/power/psci.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/rt5645.h"
#include "drivers/sound/rt5677.h"
#include "drivers/storage/mtk_mmc.h"
#include "drivers/flash/mtk_nor_flash.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

#include "drivers/video/display.h"
#include "drivers/video/mtk_ddp.h"


static GpioOps *oak_get_panel_lcd_power_en(void)
{
	switch (lib_sysinfo.board_id + CONFIG_BOARD_ID_ADJUSTMENT) {
	case 1:
	case 2:
		return NULL;
	case 3:
		return new_mtk_gpio_output(PAD_UCTS2);
	case 4:
		return new_mtk_gpio_output(PAD_SRCLKENAI);
	default:
		return new_mtk_gpio_output(PAD_UTXD2);
	}
}

int oak_backlight_update(DisplayOps *me, uint8_t enable)
{
	static GpioOps *panel_lcd_power_en, *disp_pwm0, *panel_power_en;

	if (!panel_power_en) {
		panel_lcd_power_en = oak_get_panel_lcd_power_en();
		disp_pwm0 = new_mtk_gpio_output(PAD_DISP_PWM0);
		panel_power_en = new_mtk_gpio_output(PAD_PCM_TX);
	}

	if (enable) {
		if (panel_lcd_power_en) {
			panel_lcd_power_en->set(panel_lcd_power_en, 1);
			mdelay(1);
		}

		disp_pwm0->set(disp_pwm0, 1);
		panel_power_en->set(panel_power_en, 1);
	} else {
		panel_power_en->set(panel_power_en, 0);
		disp_pwm0->set(disp_pwm0, 0);
		if (panel_lcd_power_en)
			panel_lcd_power_en->set(panel_lcd_power_en, 0);
	}

	return 0;
}

static int sound_setup(void)
{
	MtkI2s *i2s0 = new_mtk_i2s(0x11220000, 2, 48000, AFE_I2S1);
	I2sSource *i2s_source = new_i2s_source(&i2s0->ops, 48000, 2, 8000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	MTKI2c *i2c0 = new_mtk_i2c(0x11007000, 0x11000100);
	rt5645Codec *rt5645 = new_rt5645_codec(&i2c0->ops, 0x1a);

	list_insert_after(&rt5645->component.list_node, &sound_route->components);

	if (lib_sysinfo.board_id + CONFIG_BOARD_ID_ADJUSTMENT < 5) {
		rt5677Codec *rt5677 = new_rt5677_codec(&i2c0->ops, 0x2c, 16,
						       48000, 256, 0, 1);
		list_insert_after(&rt5677->component.list_node,
				  &sound_route->components);
	}

	/*
	 * Realtek codecs need SoC's I2S on before codecs are enabled
	 * or codecs' PLL will be in wrong state. Make i2s a route component
	 * so it can be enabled before codecs during route_enable_components()
	 * by inserting i2s before codec nodes
	 */
	list_insert_after(&i2s0->component.list_node, &sound_route->components);
	sound_set_ops(&sound_route->ops);
	return 0;
}

static int cr50_irq_status(void)
{
	static GpioOps *cr50_irq;

	if (!cr50_irq)
		cr50_irq = sysinfo_lookup_gpio("TPM interrupt", 1,
					       new_mtk_eint);

	return cr50_irq->get(cr50_irq);
}

static int board_setup(void)
{
	MtkMmcTuneReg emmc_tune_reg = {.msdc_iocon = 0, .pad_tune = 0x10 << 16};
	MtkMmcTuneReg sd_card_tune_reg = {.msdc_iocon = 0, .pad_tune = 0};

	sysinfo_install_flags(new_mtk_gpio_input);

	MTKI2c *i2c2 = new_mtk_i2c(0x11009000, 0x11000200);

	if (CONFIG_DRIVER_TPM_CR50_I2C)
		tpm_set_ops(&new_cr50_i2c(&i2c2->ops, 0x50,
					  &cr50_irq_status)->base.ops);
	else
		tpm_set_ops(&new_slb9635_i2c(&i2c2->ops, 0x20)->base.ops);

	GpioOps *spi_cs = new_gpio_not(new_mtk_gpio_output(PAD_MSDC2_CMD));
	MtkSpi *spibus = new_mtk_spi(0x1100A000, spi_cs);
	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spibus->ops);
	GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					      new_mtk_gpio_input);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_spi_bus->ops, 0, ec_int);
	register_vboot_ec(&cros_ec->vboot, 0);

	/* oak-rev7 / elm-rev0 onwards use ANX7688. */
	if (lib_sysinfo.board_id + CONFIG_BOARD_ID_ADJUSTMENT < 7) {
		CrosEc *cros_pd = new_cros_ec(&cros_ec_spi_bus->ops, 1, NULL);

		register_vboot_ec(&cros_pd->vboot, 1);
	} else {
		CrosECTunnelI2c *cros_ec_i2c_tunnel =
			new_cros_ec_tunnel_i2c(cros_ec, 1);

		Anx7688 *anx7688 = new_anx7688(cros_ec_i2c_tunnel);

		register_vboot_ec(&anx7688->vboot, 1);
	}

	power_set_ops(&psci_power_ops);

	MtkMmcHost *emmc = new_mtk_mmc_host(0x11230000, 200 * MHz, 50 * MHz,
					    emmc_tune_reg, 8, 0, NULL,
					    MTK_MMC_V1);
	GpioOps *card_detect_ops = new_gpio_not(new_mtk_gpio_input(PAD_EINT1));
	MtkMmcHost *sd_card = new_mtk_mmc_host(0x11240000, 200 * MHz, 25 * MHz,
					       sd_card_tune_reg, 4, 1,
					       card_detect_ops, MTK_MMC_V1);

	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);
	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	/* Set display ops */
	if (display_init_required())
		display_set_ops(new_mtk_display(
				oak_backlight_update, 0x1400c000, 1));

	UsbHostController *usb_host = new_usb_hc(XHCI, 0x11270000);

	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	sound_setup();

	/* Setup Nor flash ops */
	MtkNorFlash *nor_flash = new_mtk_nor_flash(0x1100D000);
	flash_set_ops(&nor_flash->ops);

	return 0;
}

INIT_FUNC(board_setup);

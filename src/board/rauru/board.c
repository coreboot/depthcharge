/* SPDX-License-Identifier: GPL-2.0 */

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "boot/commandline.h"
#include "drivers/bus/i2c/mtk_i2c.h"
#include "drivers/bus/spi/mtk.h"
#include "drivers/bus/i2s/mtk_v3.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/ec/rts5453/rts5453.h"
#include "drivers/flash/mtk_snfc.h"
#include "drivers/gpio/mtk_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/nau8318.h"
#include "drivers/sound/tas2563.h"
#include "drivers/storage/mtk_mmc.h"
#include "drivers/storage/mtk_ufs.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/video/display.h"
#include "drivers/video/mtk_ddp.h"
#include "vboot/util/flag.h"

static int tpm_irq_status(void)
{
	static GpioOps *tpm_int;
	if (!tpm_int)
		tpm_int = sysinfo_lookup_gpio("TPM interrupt", 1,
					      new_mtk_eint);
	assert(tpm_int);
	return gpio_get(tpm_int);
}

static void enable_usb_vbus(struct UsbHostController *usb_host)
{
	/*
	 * To avoid USB detection issue, assert GPIO AP_XHCI_INIT_DONE
	 * to notify EC to enable USB VBUS when xHCI is initialized.
	 */
	GpioOps *pdn = sysinfo_lookup_gpio("XHCI init done", 1,
					   new_mtk_gpio_output);

	if (pdn) {
		gpio_set(pdn, 1);

		/* After USB VBUS is enabled, delay 500ms for USB detection. */
		mdelay(500);
	}
}

static void setup_tas2563(GpioOps *spk_rst_l)
{
	MtkI2s *i2so1 = new_mtk_i2s(0x1A110000, 2, 32 * KHz, 16, 16, AFE_I2S_O4);
	I2sSource *i2s_source = new_i2s_source(&i2so1->ops, 32 * KHz, 2, 8000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	MTKI2c *i2c3 = new_mtk_i2c(0x13150000, 0x163c0000, I2C_APDMA_ASYNC);
	Tas2563Codec *speaker_r = new_tas2563_codec(&i2c3->ops, 0x4c, 1, 0);
	Tas2563Codec *speaker_l = new_tas2563_codec(&i2c3->ops, 0x4f, 0, 0);

	gpio_set(spk_rst_l, 0);

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
	GpioOps *spk_reset = sysinfo_lookup_gpio("speaker reset", 1,
						 new_mtk_gpio_output);
	GpioOps *spk_en = sysinfo_lookup_gpio("speaker enable", 1,
					      new_mtk_gpio_output);
	GpioOps *beep_en = sysinfo_lookup_gpio("beep enable", 1,
					       new_mtk_gpio_output);

	if (spk_en && beep_en) {
		printf("setup amps nau8318\n");
		nau8318Codec *nau8318 = new_nau8318_codec(spk_en, beep_en);
		sound_set_ops(&nau8318->ops);
	} else if (spk_reset) {
		printf("setup amps tas2563\n");
		setup_tas2563(spk_reset);
	} else {
		printf("no amps found\n");
	}
}

static int board_backlight_update(DisplayOps *me, bool enable)
{
	GpioOps *pwm_control = sysinfo_lookup_gpio("PWM control", 1,
						    new_mtk_gpio_output);
	GpioOps *backlight_en = sysinfo_lookup_gpio("backlight enable", 1,
						    new_mtk_gpio_output);

	/* TODO: Add eDP AUX for backlight control */
	if (pwm_control)
		gpio_set(pwm_control, enable);
	else
		printf("%s: Use eDP AUX to control panel backlight\n", __func__);

	if (backlight_en)
		gpio_set(backlight_en, enable);

	return 0;
}

static int board_setup(void)
{
	sysinfo_install_flags(new_mtk_gpio_input);
	power_set_ops(&psci_power_ops);

	/* Set up TPM */
	MTKI2c *i2c1 = new_mtk_i2c(0x13930000, 0x16380000, I2C_APDMA_ASYNC);
	GscI2c *tpm = new_gsc_i2c(&i2c1->ops, GSC_I2C_ADDR, &tpm_irq_status);
	tpm_set_ops(&tpm->base.ops);

	/* Set up EC */
	flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	if (!CONFIG(DETACHABLE))
		flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());

	GpioOps *spi1_cs = new_gpio_not(new_mtk_gpio_output(PAD_SPI1_CSB));
	MtkSpi *spi1 = new_mtk_spi(0x16130000, spi1_cs);
	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spi1->ops);
	GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					      new_mtk_gpio_input);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_spi_bus->ops, ec_int);
	register_vboot_ec(&cros_ec->vboot);

	sound_setup();

	/* Set up NOR flash ops */
	MtkNorFlash *nor_flash = new_mtk_nor_flash(0x16340000);
	flash_set_ops(&nor_flash->ops);

	/* Set up SD card */
	MtkMmcTuneReg sd_card_tune_reg = {
		.msdc_iocon = 0x0,
		.pad_tune = 0x0,
	};
	GpioOps *card_detect_ops = sysinfo_lookup_gpio("SD card detect", 1,
						       new_mtk_gpio_input);
	if (!card_detect_ops) {
		printf("%s: SD card detect GPIO not found\n", __func__);
	} else {
		MtkMmcHost *sd_card = new_mtk_mmc_host(0x16310000,
						       0x138a0000,
						       200 * MHz,
						       25 * MHz,
						       sd_card_tune_reg,
						       4, 1, card_detect_ops,
						       MTK_MMC_V2);

		list_insert_after(&sd_card->mmc.ctrlr.list_node,
				  &removable_block_dev_controllers);
	}

	/* Set up UFS ops */
	MtkUfsCtlr *ufs_host = new_mtk_ufs_ctlr(0x16810000, 0);
	list_insert_after(&ufs_host->ufs.bctlr.list_node, &fixed_block_dev_controllers);

	/* Set up USB */
	UsbHostController *usb_host = new_usb_hc(XHCI, 0x16700000);
	set_usb_init_callback(usb_host, enable_usb_vbus);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	/* Set display ops */
	if (display_init_required()) {
		display_set_ops(new_mtk_display(board_backlight_update,
						0x32850000, 2, 0x328e0000));
	} else {
		printf("[%s] no display_init_required()!\n", __func__);
	}

	/* Disable MTE support for ChromeOS. b:375543707 */
	commandline_append("arm64.nomte");

	return 0;
}

INIT_FUNC(board_setup);

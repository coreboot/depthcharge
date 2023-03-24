// SPDX-License-Identifier: GPL-2.0

#include <assert.h>
#include <libpayload.h>
#include "base/init_funcs.h"
#include "drivers/bus/i2c/mtk_i2c.h"
#include "drivers/bus/i2s/mtk_v2.h"
#include "drivers/bus/spi/mtk.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/mtk_snfc.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/mtk_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98390.h"
#include "drivers/sound/nau8318.h"
#include "drivers/storage/mtk_mmc.h"
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

static int board_backlight_update(DisplayOps *me, uint8_t enable)
{
	GpioOps *pwm_control = sysinfo_lookup_gpio("PWM control", 1,
						   new_mtk_gpio_output);
	GpioOps *backlight_en = sysinfo_lookup_gpio("backlight enable", 1,
						    new_mtk_gpio_output);

	/* Enforce enable to be either 0 or 1. */
	enable = !!enable;

	/* TODO: Add eDP AUX for backlight control */
	if (pwm_control)
		gpio_set(pwm_control, enable);
	else
		printf("%s: Use eDP AUX to control panel backlight\n",
		       __func__);

	if (backlight_en)
		gpio_set(backlight_en, enable);

	return 0;
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

static void setup_max98390(GpioOps *spk_rst_l)
{
	/*
	 * Currently MAX98390 only supports 24-bit word length and
	 * 16-bit bit length.
	 */
	MtkI2s *i2so1 = new_mtk_i2s(0x10b10000, 2, 32 * KHz,
				    24, 16, AFE_I2S_I1O1);
	I2sSource *i2s_source = new_i2s_source(&i2so1->ops, 32 * KHz, 2, 8000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	MTKI2c *i2c0 = new_mtk_i2c(0x11280000, 0x10220080, I2C_APDMA_ASYNC);
	Max98390Codec *speaker_r = new_max98390_codec(&i2c0->ops, 0x38);
	Max98390Codec *speaker_l = new_max98390_codec(&i2c0->ops, 0x39);

	gpio_set(spk_rst_l, 0);

	/* Delay 1ms for I2C ready */
	mdelay(1);

	list_insert_after(&speaker_l->component.list_node,
			  &sound_route->components);
	list_insert_after(&speaker_r->component.list_node,
			  &sound_route->components);

	sound_set_ops(&sound_route->ops);
}

static void setup_nau8318(GpioOps *spk_en, GpioOps *beep_en)
{
	nau8318Codec *nau8318 = new_nau8318_codec(spk_en, beep_en);
	sound_set_ops(&nau8318->ops);
}

static void sound_setup(void)
{
	GpioOps *spk_reset = sysinfo_lookup_gpio("speaker reset", 1,
						 new_mtk_gpio_output);
	GpioOps *spk_en = sysinfo_lookup_gpio("speaker enable", 1,
					      new_mtk_gpio_output);
	GpioOps *beep_en = sysinfo_lookup_gpio("beep enable", 1,
					       new_mtk_gpio_output);

	if (spk_reset)
		setup_max98390(spk_reset);
	else if (spk_en && beep_en)
		setup_nau8318(spk_en, beep_en);
}

static int board_setup(void)
{
	sysinfo_install_flags(new_mtk_gpio_input);

	/*
	 * flag_fetch() will die if it encounters a flag that is not registered,
	 * so we still register mock-up lid and power switches even if the
	 * device does not have them.
	 */
	if (CONFIG(DRIVER_EC_CROS)) {
		flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
		if (!CONFIG(DETACHABLE))
			flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());
	} else {
		flag_replace(FLAG_LIDSW, new_gpio_high());
		flag_replace(FLAG_PWRSW, new_gpio_low());
	}

	MTKI2c *i2c1 = new_mtk_i2c(0x11E00000, 0x10220100, I2C_APDMA_ASYNC);
	tpm_set_ops(&new_gsc_i2c(&i2c1->ops, GSC_I2C_ADDR,
				  &tpm_irq_status)->base.ops);

	power_set_ops(&psci_power_ops);

	/* Set up EC */
	GpioOps *spi0_cs = new_gpio_not(new_mtk_gpio_output(PAD_SPIM0_CSB));
	MtkSpi *spi0 = new_mtk_spi(0x1100A000, spi0_cs);
	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spi0->ops);
	GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					      new_mtk_gpio_input);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_spi_bus->ops, ec_int);
	register_vboot_ec(&cros_ec->vboot);

	/* Set up NOR flash ops */
	MtkNorFlash *nor_flash = new_mtk_nor_flash(0x1132C000);
	flash_set_ops(&nor_flash->ops);

	/* Set up eMMC */
	MtkMmcTuneReg emmc_tune_reg = {
		.msdc_iocon = 0x1 << 8,
		.pad_tune = 0x10 << 16
	};
	MtkMmcHost *emmc = new_mtk_mmc_host(
		0x11230000, 0x11f50000, 400 * MHz, 200 * MHz, emmc_tune_reg,
		8, 0, NULL, MTK_MMC_V2);

	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	/* Set up SD card */
	MtkMmcTuneReg sd_card_tune_reg = {
		.msdc_iocon = 0x0,
		.pad_tune = 0x0
	};
	GpioOps *card_detect_ops = sysinfo_lookup_gpio("SD card detect", 1,
						       new_mtk_gpio_input);
	if (!card_detect_ops) {
		printf("%s: SD card detect GPIO not found\n", __func__);
	} else {
		MtkMmcHost *sd_card = new_mtk_mmc_host(0x11240000,
						       0x11eb0000,
						       200 * MHz,
						       25 * MHz,
						       sd_card_tune_reg,
						       4, 1, card_detect_ops,
						       MTK_MMC_V2);

		list_insert_after(&sd_card->mmc.ctrlr.list_node,
				  &removable_block_dev_controllers);
	}

	/* Set up USB */
	UsbHostController *usb_host = new_usb_hc(XHCI, 0x11200000);
	set_usb_init_callback(usb_host, enable_usb_vbus);
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

// SPDX-License-Identifier: GPL-2.0

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/late_init_funcs.h"
#include "drivers/bus/i2s/mtk.h"
#include "drivers/bus/spi/mtk.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/spi.h"
#include "drivers/flash/mtk_snfc.h"
#include "drivers/gpio/mtk_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/sound/gpio_amp.h"
#include "drivers/sound/i2s.h"
#include "drivers/storage/mtk_mmc.h"
#include "drivers/tpm/spi.h"
#include "drivers/video/display.h"
#include "drivers/video/mtk_ddp.h"
#include "vboot/util/flag.h"

#define GPIO_XHCI_DONE	PAD_PERIPHERAL_EN1
#define GPIO_PWM	PAD_DISP_PWM
#define GPIO_BL_EN	PAD_PERIPHERAL_EN5

static SoundRouteComponent *get_speaker_amp(void)
{
	GpioOps *spk_en = sysinfo_lookup_gpio("speaker enable", 1,
					      new_mtk_gpio_output);

	/* MAX98357A, or a GPIO AMP. */
	GpioAmpCodec *codec = new_gpio_amp_codec(spk_en);

	return &codec->component;
}

static void sound_setup(void)
{
	MtkI2s *i2s2 = new_mtk_i2s(0x11210000, 2, 48000, AFE_I2S2_I2S3);
	I2sSource *i2s_source = new_i2s_source(&i2s2->ops, 48000, 2, 8000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);

	SoundRouteComponent *speaker_amp = get_speaker_amp();
	list_insert_after(&speaker_amp->list_node, &sound_route->components);
	list_insert_after(&i2s2->component.list_node,
			  &sound_route->components);

	sound_set_ops(&sound_route->ops);
}

static int cr50_irq_status(void)
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
	static GpioOps *disp_pwm0, *backlight_en;

	if (!disp_pwm0)
		disp_pwm0 = new_mtk_gpio_output(GPIO_PWM);

	if (!backlight_en)
		backlight_en = new_mtk_gpio_output(GPIO_BL_EN);

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

	/* Set up EC */
	GpioOps *spi1_cs = new_gpio_not(new_mtk_gpio_output(PAD_SPI1_CSB));
	MtkSpi *spi1 = new_mtk_spi(0x11010000, spi1_cs);
	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spi1->ops);
	GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					      new_mtk_gpio_input);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_spi_bus->ops, ec_int);
	register_vboot_ec(&cros_ec->vboot);

	/* Set up TPM */
	GpioOps *spi2_cs = new_gpio_not(new_mtk_gpio_output(PAD_SPI2_CSB));
	MtkSpi *spi2 = new_mtk_spi(0x11012000, spi2_cs);
	tpm_set_ops(&new_tpm_spi(&spi2->ops, cr50_irq_status)->ops);

	/* Set up NOR flash ops */
	MtkNorFlash *nor_flash = new_mtk_nor_flash(0x11000000);
	flash_set_ops(&nor_flash->ops);

	/* Set up eMMC */
	MtkMmcTuneReg emmc_tune_reg = {
		.msdc_iocon = 0x1 << 8,
		.pad_tune = 0x10 << 16,
	};
	MtkMmcHost *emmc = new_mtk_mmc_host(
		0x11230000, 0x11cd0000, 400 * MHz, 200 * MHz, emmc_tune_reg, 8,
		0, NULL, MTK_MMC_V2);

	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	/*
	 * Set up USB.
	 * Corsola uses USB2 port1 instead of USB2 port0.
	 */
	UsbHostController *usb_host = new_usb_hc(XHCI, 0x11280000);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	/* Set display ops */
	if (display_init_required())
		display_set_ops(new_mtk_display(board_backlight_update,
						0x14005000, 1));
	else
		printf("[%s] no display_init_required()!\n", __func__);

	sound_setup();

	return 0;
}

INIT_FUNC(board_setup);

static int enable_usb_vbus(struct LateInitFunc *init)
{
	/*
	 * To avoid USB detection issue, assert GPIO AP_XHCI_INIT_DONE
	 * to notify EC to enable USB VBUS when xHCI is initialized.
	 */
	GpioOps *pdn = new_mtk_gpio_output(GPIO_XHCI_DONE);
	gpio_set(pdn, 1);
	return 0;
}

LATE_INIT_FUNC(enable_usb_vbus);

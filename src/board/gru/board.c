/*
 * Copyright 2016 Rockchip Inc.
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
#include "boot/fit.h"
#include "boot/ramoops.h"
#include "config.h"
#include "drivers/bus/i2s/rockchip.h"
#include "drivers/bus/spi/rockchip.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/rockchip.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98357a.h"
#include "drivers/sound/route.h"
#include "drivers/storage/dw_mmc.h"
#include "drivers/storage/rk_dwmmc.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/spi.h"
#include "drivers/video/display.h"
#include "vboot/util/flag.h"

static const int emmc_sd_clock_min = 400 * 1000;
static const int emmc_clock_max = 200 * 1000 * 1000;

// Set backlight to 80% by default when on
// This mirrors default value from the kernel
// from internal_backlight_no_als_ac_brightness
#define DEFAULT_EC_BL_PWM_DUTY 80

/*
 * callback to turn on/off the backlight
 */
static int kevin_backlight_update(DisplayOps *me, uint8_t enable)
{
	return cros_ec_set_bl_pwm_duty(enable ? DEFAULT_EC_BL_PWM_DUTY : 0);
}

static DisplayOps kevin_display_ops = {
	.init = NULL,
	.backlight_update = &kevin_backlight_update,
	.stop = NULL,
};

static int board_setup(void)
{
	// Claim that we have an open lid to satisfy vboot.
	flag_replace(FLAG_LIDSW, new_gpio_high());

	// Claim that we have an power key to satisfy vboot.
	flag_replace(FLAG_PWRSW, new_gpio_low());

	// TPM on Gru is connected to SPI bus #0
	RkSpi *spi0 = new_rockchip_spi(0xff1c0000);
	tpm_set_ops(&new_tpm_spi(&spi0->ops)->ops);

	// Flash on Gru is connected to SPI bus #1.
	RkSpi *spi1 = new_rockchip_spi(0xff1d0000);
	flash_set_ops(&new_spi_flash(&spi1->ops)->ops);

	// EC is connected to SPI bus #5
	RkSpi *spi5 = new_rockchip_spi(0xff200000);
	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spi5->ops);
	GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					      new_rk_gpio_input_from_coreboot);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_spi_bus->ops, 0, ec_int);
	register_vboot_ec(&cros_ec->vboot, 0);


	SdhciHost *emmc = new_mem_sdhci_host((void *)0xfe330000,
					     SDHCI_PLATFORM_NO_EMMC_HS200 |
					     SDHCI_PLATFORM_NO_CLK_BASE,
					     emmc_sd_clock_min,
					     emmc_clock_max, 198);

	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	RockchipI2s *i2s0 = new_rockchip_i2s(0xff880000, 16, 2, 256);
	I2sSource *i2s_source = new_i2s_source(&i2s0->ops, 48000, 2, 16000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);

	/* Speaker Amp codec MAX98357A */
	GpioOps *sdmode_gpio = &new_rk_gpio_output(GPIO(1, A, 2))->ops;

	max98357aCodec *speaker_amp = new_max98357a_codec(sdmode_gpio);

	list_insert_after(&speaker_amp->component.list_node,
			  &sound_route->components);

	sound_set_ops(&sound_route->ops);

	 /*
	  * SDMMC_DET_L is different on early Kevin boards, let's just ignore
	  * them.
	  */
	RkGpio *card_detect = new_rk_gpio_input(GPIO(4, D, 0));
	GpioOps *card_detect_ops = &card_detect->ops;

	card_detect_ops = new_gpio_not(card_detect_ops);

	DwmciHost *sd_card = new_rkdwmci_host(0xfe320000, 594000000, 4, 1,
					      card_detect_ops);

	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	if (IS_ENABLED(CONFIG_GRU_USB2_BOOT_REQUIRED)) {
		/* USB2.0-EHCI*/
		UsbHostController *uhst1_ehci = new_usb_hc(EHCI, 0xfe3c0000);
		list_insert_after(&uhst1_ehci->list_node,
				  &usb_host_controllers);

		/* USB2.0-OHCI */
		UsbHostController *uhst1_ohci = new_usb_hc(OHCI, 0xfe3e0000);
		list_insert_after(&uhst1_ohci->list_node,
				  &usb_host_controllers);
	}
	/* Support both USB3.0 XHCI controllers in firmware. */
	UsbHostController *uhst0_xhci = new_usb_hc(XHCI, 0xfe800000);
	UsbHostController *uhst1_xhci = new_usb_hc(XHCI, 0xfe900000);

	list_insert_after(&uhst0_xhci->list_node, &usb_host_controllers);
	list_insert_after(&uhst1_xhci->list_node, &usb_host_controllers);

	ramoops_common_set_buffer();

	// turn on the backlight
	if (lib_sysinfo.framebuffer &&
	    lib_sysinfo.framebuffer->physical_address)
		display_set_ops(&kevin_display_ops);

	return 0;
}

INIT_FUNC(board_setup);

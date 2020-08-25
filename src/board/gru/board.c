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
#include "drivers/bus/i2c/rockchip.h"
#include "drivers/bus/i2s/rockchip.h"
#include "drivers/bus/spi/rockchip.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/rockchip.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98357a.h"
#include "drivers/sound/route.h"
#include "drivers/storage/dw_mmc.h"
#include "drivers/storage/rk_dwmmc.h"
#include "drivers/storage/rk_sdhci.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/spi.h"
#include "drivers/video/display.h"
#include "drivers/video/arctic_sand_backlight.h"
#include "drivers/video/ec_pwm_backlight.h"
#include "drivers/video/rk3399.h"
#include "vboot/util/flag.h"

static const int emmc_sd_clock_min = 400 * 1000;
static const int emmc_clock_max = 150 * 1000 * 1000;


static int cr50_irq_status(void)
{
	static GpioOps *tpm_int;

	if (!tpm_int)
		tpm_int = sysinfo_lookup_gpio("TPM interrupt", 1,
					new_rk_gpio_latched_from_coreboot);

	return gpio_get(tpm_int);
}

static int board_setup(void)
{
	RkI2c *i2c0 = NULL;
	if (CONFIG(TPM2_MODE)) {
		RkSpi *tpm_spi;

		if (CONFIG(GRU_SCARLET))
			tpm_spi = new_rockchip_spi(0xff1e0000);
		else
			tpm_spi = new_rockchip_spi(0xff1c0000);
		tpm_set_ops(&new_tpm_spi(&tpm_spi->ops, cr50_irq_status)->ops);
	} else {
		i2c0 = new_rockchip_i2c((void *)0xff3c0000);
		tpm_set_ops(&new_slb9635_i2c(&i2c0->ops, 0x20)->base.ops);
	}

	// Flash on Gru is connected to SPI bus #1.
	RkSpi *spi1 = new_rockchip_spi(0xff1d0000);
	flash_set_ops(&new_spi_flash(&spi1->ops)->ops);

	// EC is connected to SPI bus #5
	RkSpi *spi5 = new_rockchip_spi(0xff200000);
	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spi5->ops);
	GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					      new_rk_gpio_input_from_coreboot);

	CrosEc *cros_ec = new_cros_ec(&cros_ec_spi_bus->ops, ec_int);
	register_vboot_ec(&cros_ec->vboot);

	sysinfo_install_flags(new_rk_gpio_input_from_coreboot);

	// On Scarlet, we have no lid switch and the power button is handled by
	// the detachable UI via MKBP. On non-Scarlet boards, we need to read
	// the state of these from the EC whenever vboot polls them.
	if (CONFIG(GRU_SCARLET)) {
		flag_replace(FLAG_LIDSW, new_gpio_high()); /* always open */
	} else {
		flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
		flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());
	}

	power_set_ops(&psci_power_ops);

	SdhciHost *emmc = new_rk_sdhci_host(0xfe330000,
					     SDHCI_PLATFORM_SUPPORTS_HS400ES |
					     SDHCI_PLATFORM_NO_CLK_BASE,
					     emmc_sd_clock_min,
					     emmc_clock_max, 149);

	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	RockchipI2s *i2s0 = new_rockchip_i2s(0xff880000, 16, 2, 256);
	I2sSource *i2s_source = new_i2s_source(&i2s0->ops,
						48000,
						2,
						CONFIG_GRU_SPEAKER_VOLUME);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);

	max98357aCodec *speaker_amp = new_max98357a_codec(
			sysinfo_lookup_gpio("speaker enable", 1,
					    new_rk_gpio_output_from_coreboot));

	list_insert_after(&speaker_amp->component.list_node,
			  &sound_route->components);

	sound_set_ops(&sound_route->ops);

	GpioOps *sd_card_detect_gpio;
	if (CONFIG(GRU_SCARLET))
		sd_card_detect_gpio = &new_rk_gpio_input(GPIO(1, B, 3))->ops;
	else
		sd_card_detect_gpio = &new_rk_gpio_input(GPIO(4, D, 0))->ops;
	sd_card_detect_gpio = new_gpio_not(sd_card_detect_gpio);
	DwmciHost *sd_card = new_rkdwmci_host(0xfe320000, 594000000, 4, 1,
					      sd_card_detect_gpio);

	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	if (CONFIG(GRU_USB2_BOOT_REQUIRED)) {
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

	// turn on the backlight
	if (lib_sysinfo.framebuffer.physical_address != 0) {
		GpioOps *backlight = NULL;
		if (CONFIG(GRU_SCARLET))
			backlight = sysinfo_lookup_gpio("backlight", 1,
					new_rk_gpio_output_from_coreboot);
		else if (CONFIG(DRIVER_VIDEO_EC_PWM_BACKLIGHT))
			backlight = new_ec_pwm_backlight();
		else if (CONFIG(DRIVER_VIDEO_ARCTICSAND_BACKLIGHT)) {
			if (!i2c0)
				i2c0 = new_rockchip_i2c((void *)0xff3c0000);
			backlight = new_arctic_sand_backlight(&i2c0->ops, 0x30);
		}
		display_set_ops(new_rk3399_display(backlight,
				!CONFIG(GRU_MIPI_DISPLAY)));
	}
	return 0;
}

INIT_FUNC(board_setup);

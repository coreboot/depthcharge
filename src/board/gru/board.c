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
#include "config.h"
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


/**
 * Read the lid switch value from the EC
 *
 * @return 0 if lid closed, 1 if lid open or unable to read
 */
static int get_lid_switch_from_ec(GpioOps *me)
{
	uint32_t lid_open;

	if (!cros_ec_read_lid_switch(&lid_open))
		return lid_open;

	/* Assume the lid is open if we get any sort of error */
	printf("error, assuming lid is open\n");
	return 1;
}

static GpioOps *lid_open_gpio(void)
{
	GpioOps *ops = xzalloc(sizeof(*ops));

	ops->get = &get_lid_switch_from_ec;
	return ops;
}

/**
 * Read the power button value from the EC
 *
 * @return 1 if button is pressed, 0 in not pressed or unable to read
 */
static int get_power_btn_from_ec(GpioOps *me)
{
	uint32_t pwr_btn;

	if (!cros_ec_read_power_btn(&pwr_btn))
		return pwr_btn;

	/* Assume poer button is not pressed if we get any sort of error */
	printf("error, assuming power button not pressed\n");
	return 0;
}

static GpioOps *power_btn_gpio(void)
{
	GpioOps *ops = xzalloc(sizeof(*ops));

	ops->get = &get_power_btn_from_ec;
	return ops;
}

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
	GpioOps *sd_card_detect_gpio;
	GpioOps *speaker_enable_gpio;
	if (IS_ENABLED(CONFIG_GRU_SCARLET)) {
		sd_card_detect_gpio = &new_rk_gpio_input(GPIO(1, B, 3))->ops;
		speaker_enable_gpio = &new_rk_gpio_output(GPIO(0, A, 2))->ops;
	} else {
		sd_card_detect_gpio = &new_rk_gpio_input(GPIO(4, D, 0))->ops;
		speaker_enable_gpio = &new_rk_gpio_output(GPIO(1, A, 2))->ops;
	}

	RkI2c *i2c0 = NULL;
	if (IS_ENABLED(CONFIG_TPM2_MODE)) {
		RkSpi *tpm_spi;

		if (IS_ENABLED(CONFIG_GRU_SCARLET))
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

	CrosEc *cros_ec = new_cros_ec(&cros_ec_spi_bus->ops, 0, ec_int);
	register_vboot_ec(&cros_ec->vboot, 0);

	sysinfo_install_flags(new_rk_gpio_input_from_coreboot);

	// Power button and lid swith available from EC.
	flag_replace(FLAG_LIDSW, lid_open_gpio());
	flag_replace(FLAG_PWRSW, power_btn_gpio());

	power_set_ops(&psci_power_ops);

	SdhciHost *emmc = new_rk_sdhci_host((void *)0xfe330000,
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

	max98357aCodec *speaker_amp = new_max98357a_codec(speaker_enable_gpio);

	list_insert_after(&speaker_amp->component.list_node,
			  &sound_route->components);

	sound_set_ops(&sound_route->ops);

	sd_card_detect_gpio = new_gpio_not(sd_card_detect_gpio);
	DwmciHost *sd_card = new_rkdwmci_host(0xfe320000, 594000000, 4, 1,
					      sd_card_detect_gpio);

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

	// turn on the backlight
	if (lib_sysinfo.framebuffer &&
	    lib_sysinfo.framebuffer->physical_address) {
		GpioOps *backlight = NULL;
		if (IS_ENABLED(CONFIG_GRU_SCARLET))
			backlight = &new_rk_gpio_output(GPIO(4, C, 6))->ops;
		else if (IS_ENABLED(CONFIG_DRIVER_VIDEO_EC_PWM_BACKLIGHT))
			backlight = new_ec_pwm_backlight();
		else if (IS_ENABLED(CONFIG_DRIVER_VIDEO_ARCTICSAND_BACKLIGHT)) {
			if (!i2c0)
				i2c0 = new_rockchip_i2c((void *)0xff3c0000);
			backlight = new_arctic_sand_backlight(&i2c0->ops, 0x30);
		}
		display_set_ops(new_rk3399_display(backlight,
				!IS_ENABLED(CONFIG_GRU_SCARLET)));
	}
	return 0;
}

INIT_FUNC(board_setup);

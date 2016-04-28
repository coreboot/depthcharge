/*
 * Copyright 2014 Rockchip Electronics Co., Ltd.
 * Copyright 2015 Google Inc.
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

#include <arch/io.h>

#include "base/init_funcs.h"
#include "board/veyron_shark/fastboot.h"
#include "boot/fit.h"
#include "boot/ramoops.h"
#include "drivers/bus/i2c/rockchip.h"
#include "drivers/bus/i2s/rockchip.h"
#include "drivers/bus/spi/rockchip.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/block_flash.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/rockchip.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/rk808.h"
#include "drivers/power/sysinfo.h"
#include "drivers/storage/dw_mmc.h"
#include "drivers/storage/rk_mmc.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/route.h"
#include "drivers/sound/max98090.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/video/display.h"
#include "drivers/video/rockchip.h"
#include "vboot/boot_policy.h"
#include "vboot/util/flag.h"

void __attribute__((weak))
fill_fb_info(BlockDevCtrlr *bdev_ctrlr_arr[BDEV_COUNT])
{
	/* Default weak implementation. */
}

const char *hardware_name(void)
{
	return "shark";
}

static int board_setup(void)
{
	static const struct boot_policy policy[] = {
		{KERNEL_IMAGE_BOOTIMG, CMD_LINE_BOOTIMG_HDR},
		{KERNEL_IMAGE_CROS, CMD_LINE_SIGNER},
	};

	if (set_boot_policy(policy, ARRAY_SIZE(policy)) == -1)
		halt();


	RkSpi *spi2 = new_rockchip_spi(0xff130000);

	SpiFlash *flash = new_spi_flash(&spi2->ops);

	flash_set_ops(&flash->ops);
	FlashBlockDev *fbdev = block_flash_register_nor(&flash->ops);

	RkSpi *spi0 = new_rockchip_spi(0xff110000);
	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spi0->ops);
	GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					      new_rk_gpio_input_from_coreboot);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_spi_bus->ops, 0, ec_int);
	register_vboot_ec(&cros_ec->vboot, 0);

	sysinfo_install_flags(new_rk_gpio_input_from_coreboot);

	RkI2c *i2c1 = new_rockchip_i2c((void *)0xff140000);

	tpm_set_ops(&new_slb9635_i2c(&i2c1->ops, 0x20)->base.ops);

	RockchipI2s *i2s0 = new_rockchip_i2s(0xff890000, 16, 2, 256);
	I2sSource *i2s_source = new_i2s_source(&i2s0->ops, 48000, 2, 16000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	RkI2c *i2c2 = new_rockchip_i2c((void *)0xff660000);
	Max98090Codec *codec = new_max98090_codec(&i2c2->ops, 0x10, 16, 48000,
						  256, 1);
	list_insert_after(&codec->component.list_node,
			  &sound_route->components);
	sound_set_ops(&sound_route->ops);

	RkI2c *i2c0 = new_rockchip_i2c((void *)0xff650000);
	Rk808Pmic *pmic = new_rk808_pmic(&i2c0->ops, 0x1b);
	SysinfoResetPowerOps *power = new_sysinfo_reset_power_ops(&pmic->ops,
			new_rk_gpio_output_from_coreboot);
	power_set_ops(&power->ops);

	DwmciHost *emmc = new_rkdwmci_host(0xff0f0000, 594000000, 8, 0, NULL);

	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	RkGpio *card_detect = new_rk_gpio_input(GPIO(7, A, 5));
	GpioOps *card_detect_ops = &card_detect->ops;

	card_detect_ops = new_gpio_not(card_detect_ops);
	DwmciHost *sd_card = new_rkdwmci_host(0xff0c0000, 594000000, 4, 1,
					      card_detect_ops);
	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	UsbHostController *usb_host1 = new_usb_hc(DWC2, 0xff540000);

	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

	UsbHostController *usb_otg = new_usb_hc(DWC2, 0xff580000);

	list_insert_after(&usb_otg->list_node, &usb_host_controllers);

	ramoops_buffer(0x31f00000, 0x100000, 0x20000);

	if (lib_sysinfo.framebuffer != NULL) {
		GpioOps *backlight_gpio = sysinfo_lookup_gpio("backlight", 1,
			new_rk_gpio_output_from_coreboot);
		display_set_ops(new_rockchip_display(backlight_gpio));
	}

	/* Fill in fastboot related information */
	BlockDevCtrlr *bdev_arr[BDEV_COUNT] = {
		[FLASH_BDEV] = &fbdev->ctrlr,
		[MMC_BDEV] = &emmc->mmc.ctrlr,
	};
	fill_fb_info(bdev_arr);

	return 0;
}

INIT_FUNC(board_setup);

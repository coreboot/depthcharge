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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <arch/io.h>

#include "base/init_funcs.h"
#include "board/daisy/i2c_arb.h"
#include "drivers/bus/i2c/s3c24x0.h"
#include "drivers/bus/i2s/exynos5.h"
#include "drivers/bus/i2s/i2s.h"
#include "drivers/bus/spi/exynos5.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/i2c.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/exynos5250.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/exynos.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98095.h"
#include "drivers/sound/route.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/exynos_mshc.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

static uint32_t *i2c_cfg = (uint32_t *)(0x10050000 + 0x234);

static int board_setup(void)
{
	sysinfo_install_flags();

	Exynos5250Gpio *lid_switch = new_exynos5250_gpio_input(GPIO_X, 3, 5);
	Exynos5250Gpio *ec_in_rw = new_exynos5250_gpio_input(GPIO_D, 1, 7);

	flag_replace(FLAG_LIDSW, &lid_switch->ops);
	flag_install(FLAG_ECINRW, &ec_in_rw->ops);

	// The power switch is active low and needs to be inverted.
	Exynos5250Gpio *power_switch_l =
		new_exynos5250_gpio_input(GPIO_X, 1, 3);
	flag_replace(FLAG_PWRSW, new_gpio_not(&power_switch_l->ops));

	// Switch from hi speed I2C to the normal one.
	writel(0x0, i2c_cfg);

	S3c24x0I2c *i2c3 = new_s3c24x0_i2c(0x12c90000);
	S3c24x0I2c *i2c4 = new_s3c24x0_i2c(0x12ca0000);
	S3c24x0I2c *i2c7 = new_s3c24x0_i2c(0x12cd0000);

	Exynos5250Gpio *request_gpio = new_exynos5250_gpio_output(GPIO_F, 0, 3);
	Exynos5250Gpio *grant_gpio = new_exynos5250_gpio_input(GPIO_E, 0, 4);
	SnowI2cArb *arb4 = new_snow_i2c_arb(&i2c4->ops, &request_gpio->ops,
					    &grant_gpio->ops);

	CrosEcI2cBus *cros_ec_i2c_bus = new_cros_ec_i2c_bus(&arb4->ops, 0x1e);
	cros_ec_set_bus(&cros_ec_i2c_bus->ops);

	tpm_set_ops(&new_slb9635_i2c(&i2c3->ops, 0x20)->base.ops);

	Exynos5Spi *spi1 = new_exynos5_spi(0x12d30000);

	flash_set_ops(&new_spi_flash(&spi1->ops, 0x400000)->ops);

	Exynos5I2s *i2s1 = new_exynos5_i2s(0x12d60000, 16, 2, 256);
	I2sSource *i2s_source = new_i2s_source(&i2s1->ops, 48000, 2, 16000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	Max98095Codec *codec = new_max98095_codec(&i2c7->ops, 0x11, 16, 48000,
						  256, 1);
	list_insert_after(&codec->component.list_node,
			  &sound_route->components);
	sound_set_ops(&sound_route->ops);

	MshciHost *emmc = new_mshci_host(0x12200000, 400000000,
					 8, 0, 0x03030001);
	MshciHost *sd_card = new_mshci_host(0x12220000, 400000000,
					    4, 1, 0x03020001);
	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);
	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	power_set_ops(&exynos_power_ops);

	UsbHostController *usb_drd = new_usb_hc(XHCI, 0x12000000);
	UsbHostController *usb_host20 = new_usb_hc(EHCI, 0x12110000);
	UsbHostController *usb_host11 = new_usb_hc(OHCI, 0x12120000);

	list_insert_after(&usb_drd->list_node, &usb_host_controllers);
	list_insert_after(&usb_host20->list_node, &usb_host_controllers);
	list_insert_after(&usb_host11->list_node, &usb_host_controllers);

	return 0;
}

INIT_FUNC(board_setup);

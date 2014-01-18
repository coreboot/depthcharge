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

#include "base/init_funcs.h"
#include "drivers/bus/i2c/exynos5_usi.h"
#include "drivers/bus/i2s/exynos5.h"
#include "drivers/bus/spi/exynos5.h"
#include "drivers/bus/usb/exynos.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/exynos5420.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/exynos.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/route.h"
#include "drivers/sound/sound.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/dw_mmc.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

static int board_setup(void)
{
	sysinfo_install_flags();

	Exynos5420Gpio *lid_switch = new_exynos5420_gpio_input(GPIO_X, 3, 4);
	Exynos5420Gpio *ec_in_rw = new_exynos5420_gpio_input(GPIO_X, 2, 3);

	flag_replace(FLAG_LIDSW, &lid_switch->ops);
	flag_install(FLAG_ECINRW, &ec_in_rw->ops);

	// The power switch is active low and needs to be inverted.
	Exynos5420Gpio *power_switch_l =
		new_exynos5420_gpio_input(GPIO_X, 1, 2);
	flag_replace(FLAG_PWRSW, new_gpio_not(&power_switch_l->ops));

	Exynos5UsiI2c *i2c9 = new_exynos5_usi_i2c(0x12e10000, 400000);

	tpm_set_ops(&new_slb9635_i2c(&i2c9->ops, 0x20)->base.ops);

	Exynos5Spi *spi1 = new_exynos5_spi(0x12d30000);
	Exynos5Spi *spi2 = new_exynos5_spi(0x12d40000);

	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spi2->ops);
	cros_ec_set_bus(&cros_ec_spi_bus->ops);

	flash_set_ops(&new_spi_flash(&spi1->ops, 0x400000)->ops);

	Exynos5I2s *i2s0 = new_exynos5_i2s_multi(0x03830000, 16, 2, 256);
	I2sSource *i2s_source = new_i2s_source(&i2s0->ops, 48000, 2, 16000);
	sound_set_ops(&new_sound_route(&i2s_source->ops)->ops);

	DwmciHost *emmc = new_dwmci_host(0x12200000, 100000000, 8, 0,
					 DWMCI_SET_SAMPLE_CLK(1) |
					 DWMCI_SET_DRV_CLK(3) |
					 DWMCI_SET_DIV_RATIO(3));
	DwmciHost *sd_card = new_dwmci_host(0x12220000, 100000000, 4, 1,
					    DWMCI_SET_SAMPLE_CLK(1) |
					    DWMCI_SET_DRV_CLK(2) |
					    DWMCI_SET_DIV_RATIO(3));
	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);
	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	power_set_ops(&exynos_power_ops);

	UsbHostController *usb_drd0 = new_usb_hc(XHCI, 0x12000000);
	UsbHostController *usb_drd1 = new_usb_hc(XHCI, 0x12400000);

	set_usb_init_callback(usb_drd0, exynos5420_usbss_phy_tune);
	/* DRD1 port has no SuperSpeed lines anyway */

	list_insert_after(&usb_drd0->list_node, &usb_host_controllers);
	list_insert_after(&usb_drd1->list_node, &usb_host_controllers);

	return 0;
}

INIT_FUNC(board_setup);

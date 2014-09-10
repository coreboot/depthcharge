/*
 * Copyright 2014 Rockchip Electronics Co., Ltd.
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
#include "drivers/gpio/rockchip.h"
#include "drivers/bus/i2c/rockchip.h"
#include "drivers/flash/spi.h"
#include "drivers/bus/spi/rockchip.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "board/veyron_pinky/power_ops.h"
#include "drivers/power/rk808.h"
#include "drivers/storage/dw_mmc.h"
#include "drivers/storage/rk_mmc.h"

#include "drivers/gpio/sysinfo.h"
#include "vboot/util/flag.h"
#include "drivers/bus/i2s/rockchip.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/route.h"
#include "drivers/sound/max98090.h"

#include "drivers/bus/usb/usb.h"

static int board_setup(void)
{
	RkSpi *spi2 = new_rockchip_spi(0xff130000, 0, 0, 0);
	flash_set_ops(&new_spi_flash(&spi2->ops, 0x400000)->ops);

	RkSpi *spi0 = new_rockchip_spi(0xff110000, 0, 0, 0);
	cros_ec_set_bus(&new_cros_ec_spi_bus(&spi0->ops)->ops);

	sysinfo_install_flags();
	RkGpio *lid_switch = new_rk_gpio_input((RkGpioSpec) {.port = 7,
							     .bank = GPIO_B,
							     .idx = 5});
	RkGpio *ec_in_rw = new_rk_gpio_input((RkGpioSpec) {.port = 0,
							   .bank = GPIO_A,
							   .idx = 7});
	flag_replace(FLAG_LIDSW, &lid_switch->ops);
	flag_install(FLAG_ECINRW, &ec_in_rw->ops);

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
	RkGpio *reboot_gpio = new_rk_gpio_output((RkGpioSpec) {.port = 0,
							       .bank = GPIO_B,
							       .idx = 2});
	RkPowerOps *rk_power_ops = new_rk_power_ops(&reboot_gpio->ops,
		&pmic->ops, 1);
	power_set_ops(&rk_power_ops->ops);

	DwmciHost *emmc = new_rkdwmci_host(0xff0f0000, 594000000, 8, 0, NULL);
	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	RkGpio *card_detect = new_rk_gpio_input((RkGpioSpec) {.port = 7,
							      .bank = GPIO_A,
							      .idx = 5});
	GpioOps *card_detect_ops = &card_detect->ops;
	card_detect_ops = new_gpio_not(card_detect_ops);
	DwmciHost *sd_card = new_rkdwmci_host(0xff0c0000, 594000000, 4, 1,
					      card_detect_ops);
	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	UsbHostController *usb_host0 = new_usb_hc(DWC2, 0xff580000);
	list_insert_after(&usb_host0->list_node, &usb_host_controllers);

	UsbHostController *usb_host1 = new_usb_hc(DWC2, 0xff540000);
	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

	return 0;
}

INIT_FUNC(board_setup);

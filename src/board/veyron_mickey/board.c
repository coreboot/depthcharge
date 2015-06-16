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
#include "boot/fit.h"
#include "boot/ramoops.h"
#include "drivers/bus/i2c/rockchip.h"
#include "drivers/bus/i2s/rockchip.h"
#include "drivers/bus/spi/rockchip.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/rockchip.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/rk808.h"
#include "drivers/power/sysinfo.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/route.h"
#include "drivers/storage/dw_mmc.h"
#include "drivers/storage/rk_mmc.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/video/display.h"
#include "drivers/video/rockchip.h"
#include "vboot/util/flag.h"

static int board_setup(void)
{
	fit_set_compat_by_rev("google,veyron-mickey-rev%d",
			      lib_sysinfo.board_id);

	RkSpi *spi2 = new_rockchip_spi(0xff130000);
	flash_set_ops(&new_spi_flash(&spi2->ops)->ops);

	sysinfo_install_flags(new_rk_gpio_input_from_coreboot);

	RkI2c *i2c1 = new_rockchip_i2c((void *)0xff140000);
	tpm_set_ops(&new_slb9635_i2c(&i2c1->ops, 0x20)->base.ops);

	RockchipI2s *i2s0 = new_rockchip_i2s(0xff890000, 16, 2, 256);
	I2sSource *i2s_source = new_i2s_source(&i2s0->ops, 48000, 2, 16000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	sound_set_ops(&sound_route->ops);

	RkI2c *i2c0 = new_rockchip_i2c((void *)0xff650000);
	Rk808Pmic *pmic = new_rk808_pmic(&i2c0->ops, 0x1b);
	SysinfoResetPowerOps *power = new_sysinfo_reset_power_ops(&pmic->ops,
			new_rk_gpio_output_from_coreboot);
	power_set_ops(&power->ops);

	DwmciHost *emmc = new_rkdwmci_host(0xff0f0000, 594000000, 8, 0, NULL);
	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	// This is actually an OTG port and is labeled as such in the schematic,
	// though in reality we use it as a regular host mode port and leave
	// the OTG_ID pin disconnected.
	UsbHostController *usb_otg = new_usb_hc(DWC2, 0xff580000);
	list_insert_after(&usb_otg->list_node, &usb_host_controllers);

	// Claim that we have an open lid to satisfy vboot.
	flag_replace(FLAG_LIDSW, new_gpio_high());

	// Claim that we have an power key to satisfy vboot.
	flag_replace(FLAG_PWRSW, new_gpio_low());

	ramoops_buffer(0x31f00000, 0x100000, 0x20000);

	if (lib_sysinfo.framebuffer != NULL)
		display_set_ops(NULL);

	return 0;
}

INIT_FUNC(board_setup);

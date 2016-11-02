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
#include <config.h>
#include <libpayload.h>
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "base/device_tree.h"
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
#include "drivers/sound/gpio_buzzer.h"
#include "drivers/storage/dw_mmc.h"
#include "drivers/storage/rk_mmc.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/video/display.h"
#include "drivers/video/rockchip.h"
#include "vboot/util/flag.h"

/* On-board MAC in the device-tree */
#define DT_PATH_GMAC "ethernet@ff290000"

/*
 * MAC address fixup.
 * Map the MAC address found in the coreboot table into the device tree
 * gmac peripheral settings.
 */
static int fix_dt_mac_addr(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	DeviceTreeNode *gmac_node;

	if (lib_sysinfo.num_macs < 1) {
		printf("No MAC address defined in the VPD\n");
		return 0;
	}

	gmac_node = dt_find_node_by_path(tree->root, DT_PATH_GMAC,
					 NULL, NULL, 0);
	if (!gmac_node) {
		printf("Failed to find %s in the device tree\n", DT_PATH_GMAC);
		return 0;
	}
	dt_add_bin_prop(gmac_node, "mac-address",
			lib_sysinfo.macs[0].mac_addr,
			sizeof(lib_sysinfo.macs[0].mac_addr));

	return 0;
}

static DeviceTreeFixup mac_addr_fixup = {
	.fixup = fix_dt_mac_addr
};

static int board_setup(void)
{
	fit_set_compat_by_rev("google,veyron-fievel-rev%d",
			      lib_sysinfo.board_id);

	RkSpi *spi2 = new_rockchip_spi(0xff130000);
	flash_set_ops(&new_spi_flash(&spi2->ops, 0x400000)->ops);

	sysinfo_install_flags(new_rk_gpio_input_from_coreboot);

	RkI2c *i2c1 = new_rockchip_i2c((void *)0xff140000);
	tpm_set_ops(&new_slb9635_i2c(&i2c1->ops, 0x20)->base.ops);

	RkGpio *buzzer = new_rk_gpio_output(GPIO(7, A, 5));
	GpioOps *buzzer_ops = &buzzer->ops;
	sound_set_ops(&new_gpio_buzzer_sound(buzzer_ops)->ops);

	RkI2c *i2c0 = new_rockchip_i2c((void *)0xff650000);
	Rk808Pmic *pmic = new_rk808_pmic(&i2c0->ops, 0x1b);
	SysinfoResetPowerOps *power = new_sysinfo_reset_power_ops(&pmic->ops,
			new_rk_gpio_output_from_coreboot);
	power_set_ops(&power->ops);

	DwmciHost *emmc = new_rkdwmci_host(0xff0f0000, 594000000, 8, 0, NULL);
	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	UsbHostController *usb_host1 = new_usb_hc(DWC2, 0xff540000);
	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

	// This is actually an OTG port and is labeled as such in the schematic,
	// though in reality we use it as a regular host mode port and leave
	// the OTG_ID pin disconnected.
	UsbHostController *usb_otg = new_usb_hc(DWC2, 0xff580000);
	list_insert_after(&usb_otg->list_node, &usb_host_controllers);

	// Claim that we have an open lid to satisfy vboot.
	flag_replace(FLAG_LIDSW, new_gpio_high());

	// Read the current value of the recovery button for confirmation
	// when transitioning between normal and dev mode.
	flag_replace(FLAG_RECSW, sysinfo_lookup_gpio("recovery",
				1, new_rk_gpio_input_from_coreboot));

	list_insert_after(&mac_addr_fixup.list_node, &device_tree_fixups);

	ramoops_buffer(0x31f00000, 0x100000, 0x20000);

	if (lib_sysinfo.framebuffer != NULL)
		display_set_ops(NULL);

	return 0;
}

INIT_FUNC(board_setup);

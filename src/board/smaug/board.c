/*
 * Copyright 2015 Google Inc.
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

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "boot/fit.h"
#include "boot/commandline.h"
#include "board/smaug/fastboot.h"
#include "config.h"
#include "drivers/bus/spi/tegra.h"
#include "drivers/bus/i2c/tegra.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/tegra210.h"
#include "drivers/dma/tegra_apb.h"
#include "drivers/flash/block_flash.h"
#include "drivers/flash/spi.h"
#include "drivers/power/sysinfo.h"
#include "drivers/power/max77620.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/storage/tegra_mmc.h"
#include "drivers/video/display.h"
#include "drivers/ec/cros/i2c.h"
#include "vboot/boot_policy.h"
#include "vboot/util/flag.h"

enum {
	CLK_RST_BASE = 0x60006000,

	CLK_RST_L_RST_SET = CLK_RST_BASE + 0x300,
	CLK_RST_L_RST_CLR = CLK_RST_BASE + 0x304,
	CLK_RST_H_RST_SET = CLK_RST_BASE + 0x308,
	CLK_RST_H_RST_CLR = CLK_RST_BASE + 0x30c,
	CLK_RST_U_RST_SET = CLK_RST_BASE + 0x310,
	CLK_RST_U_RST_CLR = CLK_RST_BASE + 0x314,
	CLK_RST_X_RST_SET = CLK_RST_BASE + 0x290,
	CLK_RST_X_RST_CLR = CLK_RST_BASE + 0x294,
	CLK_RST_Y_RST_SET = CLK_RST_BASE + 0x2a8,
	CLK_RST_Y_RST_CLR = CLK_RST_BASE + 0x2ac,
};

enum {
	CLK_L_I2C1 = 0x1 << 12,
	CLK_H_I2C2 = 0x1 << 22,
	CLK_U_I2C3 = 0x1 << 3,
	CLK_H_I2C5 = 0x1 << 15,
	CLK_X_I2C6 = 0x1 << 6
};

const char *mainboard_commandline(void)
{
	return NULL;
}

const char *hardware_name(void)
{
	return "dragon";
}

static void choose_devicetree_by_boardid(void)
{
	fit_set_compat_by_rev("google,smaug-rev%d", lib_sysinfo.board_id);
}

static int board_setup(void)
{
	sysinfo_install_flags(new_tegra_gpio_input_from_coreboot);

	choose_devicetree_by_boardid();

	static const struct boot_policy policy[] = {
		{KERNEL_IMAGE_BOOTIMG, CMD_LINE_DTB},
		{KERNEL_IMAGE_CROS, CMD_LINE_SIGNER},
	};

	if (set_boot_policy(policy, ARRAY_SIZE(policy)) == -1)
		halt();

	void *dma_channel_bases[32];
	for (int i = 0; i < ARRAY_SIZE(dma_channel_bases); i++)
		dma_channel_bases[i] = (void *)((unsigned long)0x60021000
						+ 0x40 * i);

	TegraApbDmaController *dma_controller =
		new_tegra_apb_dma((void *)0x60020000, dma_channel_bases,
				  ARRAY_SIZE(dma_channel_bases));

	TegraSpi *qspi = new_tegra_spi(0x70410000, dma_controller,
				       APBDMA_SLAVE_QSPI);

	SpiFlash *flash = new_spi_flash(&qspi->ops);

	flash_set_ops(&flash->ops);

	FlashBlockDev *fbdev = block_flash_register_nor(&flash->ops);

	TegraI2c *gen3_i2c = new_tegra_i2c((void *)0x7000c500, 3,
					   (void *)CLK_RST_U_RST_SET,
					   (void *)CLK_RST_U_RST_CLR,
					   CLK_U_I2C3);

	tpm_set_ops(&new_slb9635_i2c(&gen3_i2c->ops, 0x20)->base.ops);

	TegraI2c *ec_i2c = new_tegra_i2c((void *)0x7000c400, 2,
					  (void *)CLK_RST_H_RST_SET,
					  (void *)CLK_RST_H_RST_CLR,
					  CLK_H_I2C2);

	CrosEcI2cBus *cros_ec_i2c_bus = new_cros_ec_i2c_bus(&ec_i2c->ops, 0x1E);
	cros_ec_set_bus(&cros_ec_i2c_bus->ops);

	TegraI2c *pwr_i2c = new_tegra_i2c((void *)0x7000d000, 5,
					  (void *)CLK_RST_H_RST_SET,
					  (void *)CLK_RST_H_RST_CLR,
					  CLK_H_I2C5);

	Max77620Pmic *pmic = new_max77620_pmic(&pwr_i2c->ops, 0x3c);
	SysinfoResetPowerOps *power = new_sysinfo_reset_power_ops(&pmic->ops,
			new_tegra_gpio_output_from_coreboot);
	power_set_ops(&power->ops);

	/* sdmmc4 */
	TegraMmcHost *emmc = new_tegra_mmc_host(0x700b0600, 8, 0, NULL, NULL);

	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	/* Fill in fastboot related information */
	BlockDevCtrlr *bdev_arr[BDEV_COUNT] = {
		[FLASH_BDEV] = &fbdev->ctrlr,
		[MMC_BDEV] = &emmc->mmc.ctrlr,
	};
	fill_fb_info(bdev_arr);

	/* Careful: the EHCI base is at offset 0x100 from the SoC's IP base */
	UsbHostController *usbd = new_usb_hc(EHCI, 0x7d000100);

	list_insert_after(&usbd->list_node, &usb_host_controllers);

	/* Lid always open for now. */
	flag_replace(FLAG_LIDSW, new_gpio_high());

	return 0;
}

INIT_FUNC(board_setup);

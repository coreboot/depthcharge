/*
 * Copyright 2014 Google Inc.
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
#include "board/rush_ryu/power_ops.h"
#include "boot/fit.h"
#include "boot/commandline.h"
#include "boot/ramoops.h"
#include "config.h"
#include "drivers/bus/spi/tegra.h"
#include "drivers/bus/i2c/tegra.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/tegra.h"
#include "drivers/dma/tegra_apb.h"
#include "drivers/flash/spi.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/storage/tegra_mmc.h"
#include "drivers/ec/cros/i2c.h"
#include "vboot/util/flag.h"

enum {
	BOARD_ID_PROTO_0 = 0,
	BOARD_ID_PROTO_1 = 1,
	BOARD_ID_EVT = 2,
	BOARD_ID_DVT = 3,
	BOARD_ID_PVT = 4,
	BOARD_ID_MP = 5,
};

enum {
	CLK_RST_BASE = 0x60006000,

	CLK_RST_L_RST_SET = CLK_RST_BASE + 0x300,
	CLK_RST_L_RST_CLR = CLK_RST_BASE + 0x304,
	CLK_RST_H_RST_SET = CLK_RST_BASE + 0x308,
	CLK_RST_H_RST_CLR = CLK_RST_BASE + 0x30c,
	CLK_RST_U_RST_SET = CLK_RST_BASE + 0x310,
	CLK_RST_U_RST_CLR = CLK_RST_BASE + 0x314
};

enum {
	CLK_L_I2C1 = 0x1 << 12,
	CLK_H_I2C2 = 0x1 << 22,
	CLK_U_I2C3 = 0x1 << 3,
	CLK_H_I2C5 = 0x1 << 15
};

static int lid_get_always_open (struct GpioOps *me)
{
	return 1;
}

static GpioOps always_open_lid = {
	.get = lid_get_always_open,
};

const char *mainboard_commandline(void)
{
	return NULL;
}

static void choose_devicetree_by_boardid(void)
{
	switch(lib_sysinfo.board_id) {
	case BOARD_ID_PROTO_0:
		fit_set_compat("google,ryu-p0");
		break;
	case BOARD_ID_PROTO_1:
		fit_set_compat("google,ryu-p1");
		break;
	default:
		printf("Unknown board id: %x\n", lib_sysinfo.board_id);
		break;
	}
}

static int board_setup(void)
{
	sysinfo_install_flags();

	choose_devicetree_by_boardid();

	void *dma_channel_bases[32];
	for (int i = 0; i < ARRAY_SIZE(dma_channel_bases); i++)
		dma_channel_bases[i] = (void *)((unsigned long)0x60021000 + 0x40 * i);

	TegraApbDmaController *dma_controller =
		new_tegra_apb_dma((void *)0x60020000, dma_channel_bases,
				  ARRAY_SIZE(dma_channel_bases));

	TegraSpi *spi4 = new_tegra_spi(0x7000da00, dma_controller,
				       APBDMA_SLAVE_SL2B4);

	flash_set_ops(&new_spi_flash(&spi4->ops)->ops);

	TegraI2c *cam_i2c = new_tegra_i2c((void *)0x7000c500, 3,
					  (void *)CLK_RST_U_RST_SET,
					  (void *)CLK_RST_U_RST_CLR,
					  CLK_U_I2C3);

	tpm_set_ops(&new_slb9635_i2c(&cam_i2c->ops, 0x20)->base.ops);

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

	Tps65913Pmic *pmic = new_tps65913_pmic(&pwr_i2c->ops, 0x58);

	TegraGpio *reboot_gpio = new_tegra_gpio_output(GPIO(I, 5));
	RyuPowerOps *power = new_ryu_power_ops(&pmic->ops, &reboot_gpio->ops,
						 0);
	power_set_ops(&power->ops);

	/* sdmmc4 */
	TegraMmcHost *emmc = new_tegra_mmc_host(0x700b0600, 8, 0, NULL, NULL);

	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	/* Careful: the EHCI base is at offset 0x100 from the SoC's IP base */
	UsbHostController *usbd = new_usb_hc(EHCI, 0x7d000100);

	list_insert_after(&usbd->list_node, &usb_host_controllers);

	/* Lid always open for now. */
	flag_replace(FLAG_LIDSW, &always_open_lid);

	return 0;
}

INIT_FUNC(board_setup);

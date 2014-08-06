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
#include "board/rush/power_ops.h"
#include "boot/fit.h"
#include "boot/ramoops.h"
#include "config.h"
#include "drivers/bus/spi/tegra.h"
#include "drivers/bus/i2c/tegra.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/tegra.h"
#include "drivers/dma/tegra_apb.h"
#include "drivers/flash/spi.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/storage/tegra_mmc.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/bus/usb/usb.h"

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
	CLK_U_I2C3 = 0x1 << 3,
	CLK_H_I2C5 = 0x1 << 15
};

typedef struct VirtualMmcPowerGpio
{
	GpioOps ops;

	GpioOps *gpio;
	As3722Pmic *as3722;
	uint8_t reg, enable_val, disable_val;  // Params for as3722.
} VirtualMmcPowerGpio;

static int virtual_mmc_power_set(GpioOps *me, unsigned value)
{
	VirtualMmcPowerGpio *power = container_of(me, VirtualMmcPowerGpio, ops);
	assert(power->gpio && power->as3722);
	if (power->gpio->set(power->gpio, value) ||
	    power->as3722->set_reg(power->as3722, power->reg, value ?
				   power->enable_val : power->disable_val)) {
		printf("Failed to enable SD/MMC power.\n");
		return -1;
	}
	return 0;
}

static VirtualMmcPowerGpio *new_virtual_mmc_power(GpioOps *gpio,
						  As3722Pmic *as3722,
						  uint8_t reg,
						  uint8_t enable_val,
						  uint8_t disable_val)
{
	VirtualMmcPowerGpio *power = xzalloc(sizeof(*power));
	power->gpio = gpio;
	power->as3722 = as3722;
	power->reg = reg;
	power->enable_val = enable_val;
	power->disable_val = disable_val;
	power->ops.set = &virtual_mmc_power_set;
	return power;
}

static int board_setup(void)
{
	sysinfo_install_flags();

	void *dma_channel_bases[32];
	for (int i = 0; i < ARRAY_SIZE(dma_channel_bases); i++)
		dma_channel_bases[i] = (void *)((unsigned long)0x60021000 + 0x40 * i);

	TegraApbDmaController *dma_controller =
		new_tegra_apb_dma((void *)0x60020000, dma_channel_bases,
				  ARRAY_SIZE(dma_channel_bases));

	TegraSpi *spi4 = new_tegra_spi(0x7000da00, dma_controller,
				       APBDMA_SLAVE_SL2B4);

	flash_set_ops(&new_spi_flash(&spi4->ops, 0x400000)->ops);

	TegraI2c *cam_i2c = new_tegra_i2c((void *)0x7000c500, 3,
					  (void *)CLK_RST_U_RST_SET,
					  (void *)CLK_RST_U_RST_CLR,
					  CLK_U_I2C3);

	tpm_set_ops(&new_slb9635_i2c(&cam_i2c->ops, 0x20)->base.ops);

	TegraSpi *spi1 = new_tegra_spi(0x7000d400, dma_controller,
				       APBDMA_SLAVE_SL2B1);

	cros_ec_set_bus(&new_cros_ec_spi_bus(&spi1->ops)->ops);

	TegraI2c *pwr_i2c = new_tegra_i2c((void *)0x7000d000, 5,
					  (void *)CLK_RST_H_RST_SET,
					  (void *)CLK_RST_H_RST_CLR,
					  CLK_H_I2C5);
	As3722Pmic *pmic = new_as3722_pmic(&pwr_i2c->ops, 0x40);
	TegraGpio *reboot_gpio = new_tegra_gpio_output(GPIO(I, 5));
	RushPowerOps *power = new_rush_power_ops(&pmic->ops, &reboot_gpio->ops,
						 0);
	power_set_ops(&power->ops);

	// sdmmc4
	TegraMmcHost *emmc = new_tegra_mmc_host(0x700b0600, 8, 0, NULL, NULL);
	// sdmmc3
	TegraGpio *enable_vdd_sd = new_tegra_gpio_output(GPIO(R, 0));
	// The params in mmc_power set AS3722_LDO6 to 3.3V.
	VirtualMmcPowerGpio *mmc_power = new_virtual_mmc_power(
			&enable_vdd_sd->ops, pmic, 0x16, 0x3F, 0);
	TegraGpio *card_detect = new_tegra_gpio_input(GPIO(V, 2));
	GpioOps *card_detect_ops = &card_detect->ops;
	card_detect_ops = new_gpio_not(card_detect_ops);
	TegraMmcHost *sd_card = new_tegra_mmc_host(0x700b0400, 4, 1,
						   card_detect_ops,
						   &mmc_power->ops);

	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);
	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	/* Careful: the EHCI base is at offset 0x100 from the SoC's IP base */
	UsbHostController *usbd = new_usb_hc(EHCI, 0x7d000100);
	/* USB2 is connected to the camera, not needed in firmware */
	UsbHostController *usb3 = new_usb_hc(EHCI, 0x7d008100);

	list_insert_after(&usbd->list_node, &usb_host_controllers);
	list_insert_after(&usb3->list_node, &usb_host_controllers);

	ramoops_buffer(0x87f00000, 0x100000, 0x20000);

	return 0;
}

INIT_FUNC(board_setup);

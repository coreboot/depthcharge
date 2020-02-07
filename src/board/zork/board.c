/*
 * Copyright 2019 Google Inc.
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

#include <arch/msr.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/ec/anx3429/anx3429.h"
#include "drivers/ec/ps8751/ps8751.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/kern.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/fch.h"
#include "drivers/soc/picasso.h"
#include "drivers/sound/gpio_i2s.h"
#include "drivers/sound/max98357a.h"
#include "drivers/sound/route.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/bayhub.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/video/display.h"
#include "drivers/bus/usb/usb.h"
#include "pci.h"
#include "vboot/util/flag.h"

/* SD Controllers */
#define BH720_PCI_VID		0x1217
#define BH720_PCI_DID		0x8621

/* I2S Beep GPIOs */
#define I2S_BCLK_GPIO		139
#define I2S_LRCLK_GPIO		8
#define I2S_DATA_GPIO		135

/* SPI Flash */
#define FLASH_SIZE		0x1000000	/* 16MB */
#define FLASH_START		( 0xffffffff - FLASH_SIZE + 1 )

/* Core boost register */
#define HW_CONFIG_REG 0xc0010015
#define   HW_CONFIG_CPBDIS (1 << 25)

/* cr50's interrupt is attached to GPIO_3 */
#define CR50_INT		3

/* eDP backlight */
#define GPIO_BACKLIGHT		85

static int cr50_irq_status(void)
{
	static KernGpio *tpm_gpio;

	if (!tpm_gpio)
		tpm_gpio = new_kern_fch_gpio_latched(CR50_INT);

	return gpio_get(&tpm_gpio->ops);
}

static int zork_backlight_update(DisplayOps *me, uint8_t enable)
{
	/* Backlight signal is active low */
	static KernGpio *backlight;

	if (!backlight)
		backlight = new_kern_fch_gpio_output(GPIO_BACKLIGHT, !enable);
	else
		backlight->ops.set(&backlight->ops, !enable);

	return 0;
}

static DisplayOps zork_display_ops = {
	.backlight_update = &zork_backlight_update,
};

static int board_setup(void)
{
	sysinfo_install_flags(NULL);
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, 0, NULL);
	register_vboot_ec(&cros_ec->vboot, 0);

	flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());

	flash_set_ops(&new_mem_mapped_flash(FLASH_START, FLASH_SIZE)->ops);

	SdhciHost *sd = NULL;
	pcidev_t pci_dev;
	if (pci_find_device(BH720_PCI_VID, BH720_PCI_DID, &pci_dev)) {
		sd = new_bayhub_sdhci_host(pci_dev,
				SDHCI_PLATFORM_REMOVABLE,
				0, 0);
	}

	if (sd) {
		sd->name = "SD";
		list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
				&removable_block_dev_controllers);
	} else {
		printf("Failed to find SD card reader\n");
	}

	SdhciHost *emmc = new_mem_sdhci_host(
		EMMCCFG,
		/* Can't enable HS200 or HS400 until tuning is fixed */
		SDHCI_PLATFORM_NO_EMMC_HS200 | SDHCI_PLATFORM_EMMC_1V8_POWER, 0,
		0, 0);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	/* PCI Bridge for NVMe */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(0, 0x01, 0x07));
	list_insert_after(&nvme->ctrlr.list_node,
				&fixed_block_dev_controllers);

	/* Set up h1 on I2C3 */
	DesignwareI2c *i2c_h1 = new_designware_i2c(
		AP_I2C3_ADDR, 400000, AP_I2C_CLK_MHZ);
	tpm_set_ops(&new_cr50_i2c(&i2c_h1->ops, 0x50,
				  &cr50_irq_status)->base.ops);

	power_set_ops(&kern_power_ops);
	display_set_ops(&zork_display_ops);

	return 0;
}

INIT_FUNC(board_setup);

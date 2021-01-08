// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2021 Google Inc.
 *
 * Board file for Cherry.
 */

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/flash/mtk_snfc.h"
#include "drivers/gpio/mtk_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/storage/mtk_mmc.h"

static int board_setup(void)
{
	sysinfo_install_flags(new_mtk_gpio_input);
	power_set_ops(&psci_power_ops);

	/* Setup NOR flash ops */
	MtkNorFlash *nor_flash = new_mtk_nor_flash(0x1132C000);
	flash_set_ops(&nor_flash->ops);

	MtkMmcTuneReg emmc_tune_reg = {
		.msdc_iocon = 0x1 << 8,
		.pad_tune = 0x10 << 16
	};
	MtkMmcHost *emmc = new_mtk_mmc_host(
		0x11230000, 400 * MHz, 50 * MHz, emmc_tune_reg, 8, 0, NULL,
		MTK_MMC_V2);

	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	MtkMmcTuneReg sd_card_tune_reg = {
		.msdc_iocon = 0x0,
		.pad_tune = 0x0
	};
	GpioOps *card_detect_ops = new_gpio_not(new_mtk_gpio_input(PAD_I2SO1_D1));
	MtkMmcHost *sd_card = new_mtk_mmc_host(
		0x11240000, 200 * MHz, 25 * MHz, sd_card_tune_reg, 4, 1,
		card_detect_ops, MTK_MMC_V2);

	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	UsbHostController *usb_host = new_usb_hc(XHCI, 0x11200000);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	return 0;
}

INIT_FUNC(board_setup);

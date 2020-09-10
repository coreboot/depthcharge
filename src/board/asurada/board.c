// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google Inc.
 *
 * Board file for Asurada.
 */

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "drivers/bus/i2s/mtk.h"
#include "drivers/bus/spi/mtk.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/spi.h"
#include "drivers/flash/mtk_snfc.h"
#include "drivers/gpio/mtk_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98357a.h"
#include "drivers/storage/mtk_mmc.h"
#include "drivers/tpm/spi.h"
#include "vboot/util/flag.h"

#include "drivers/video/display.h"
#include "drivers/video/mtk_ddp.h"


static int cr50_irq_status(void)
{
	static GpioOps *tpm_int;

	if (!tpm_int)
		tpm_int = sysinfo_lookup_gpio("TPM interrupt", 1,
					      new_mtk_eint);
	assert(tpm_int);
	return gpio_get(tpm_int);
}

int board_backlight_update(DisplayOps *me, uint8_t enable)
{
	return 0;
}

static int board_setup(void)
{
	sysinfo_install_flags(new_mtk_gpio_input);
	flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	if (!CONFIG(DETACHABLE))
		flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());

	power_set_ops(&psci_power_ops);

	GpioOps *spi5_cs = new_gpio_not(new_mtk_gpio_output(PAD_SPI5_CSB));
	MtkSpi *spi5 = new_mtk_spi(0x11019000, spi5_cs);
	tpm_set_ops(&new_tpm_spi(&spi5->ops, cr50_irq_status)->ops);

	GpioOps *spi1_cs = new_gpio_not(new_mtk_gpio_output(PAD_SPI1_CSB));
	MtkSpi *spi1 = new_mtk_spi(0x11010000, spi1_cs);
	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spi1->ops);
	GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					      new_mtk_gpio_input);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_spi_bus->ops, ec_int);
	register_vboot_ec(&cros_ec->vboot);

	/* Setup NOR flash ops */
	MtkNorFlash *nor_flash = new_mtk_nor_flash(0x11234000);
	flash_set_ops(&nor_flash->ops);

	MtkMmcTuneReg emmc_tune_reg = {
		.msdc_iocon = 0x1 << 8,
		.pad_tune = 0x10 << 16
	};
	MtkMmcHost *emmc = new_mtk_mmc_host(
		0x11f60000, 400 * MHz, 50 * MHz, emmc_tune_reg, 8, 0, NULL,
		MTK_MMC_V2);

	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	MtkMmcTuneReg sd_card_tune_reg = {
		.msdc_iocon = 0x0,
		.pad_tune = 0x0
	};
	GpioOps *card_detect_ops = new_gpio_not(new_mtk_gpio_input(PAD_EINT17));
	MtkMmcHost *sd_card = new_mtk_mmc_host(
		0x11f70000, 200 * MHz, 25 * MHz, sd_card_tune_reg, 4, 1,
		card_detect_ops, MTK_MMC_V2);

	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	UsbHostController *usb_host = new_usb_hc(XHCI, 0x11200000);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	/* Set display ops */
	if (lib_sysinfo.framebuffer.physical_address != 0)
		display_set_ops(new_mtk_display(board_backlight_update,
						0x14005000, 2));

	return 0;
}

INIT_FUNC(board_setup);

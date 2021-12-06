// SPDX-License-Identifier: GPL-2.0

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/late_init_funcs.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/flash/mtk_snfc.h"
#include "drivers/gpio/mtk_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/storage/mtk_mmc.h"

#define GPIO_XHCI_DONE	PAD_PERIPHERAL_EN1

static int board_setup(void)
{
	sysinfo_install_flags(new_mtk_gpio_input);
	power_set_ops(&psci_power_ops);

	/* Set up NOR flash ops */
	MtkNorFlash *nor_flash = new_mtk_nor_flash(0x11000000);
	flash_set_ops(&nor_flash->ops);

	/* Set up eMMC */
	MtkMmcTuneReg emmc_tune_reg = {
		.msdc_iocon = 0x1 << 8,
		.pad_tune = 0x10 << 16,
	};
	MtkMmcHost *emmc = new_mtk_mmc_host(
		0x11230000, 0x11cd0000, 400 * MHz, 200 * MHz, emmc_tune_reg, 8,
		0, NULL, MTK_MMC_V2);

	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	/*
	 * Set up USB.
	 * Corsola uses USB2 port1 instead of USB2 port0.
	 */
	UsbHostController *usb_host = new_usb_hc(XHCI, 0x11280000);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	return 0;
}

INIT_FUNC(board_setup);

static int enable_usb_vbus(struct LateInitFunc *init)
{
	/*
	 * To avoid USB detection issue, assert GPIO AP_XHCI_INIT_DONE
	 * to notify EC to enable USB VBUS when xHCI is initialized.
	 */
	GpioOps *pdn = new_mtk_gpio_output(GPIO_XHCI_DONE);
	gpio_set(pdn, 1);
	return 0;
}

LATE_INIT_FUNC(enable_usb_vbus);

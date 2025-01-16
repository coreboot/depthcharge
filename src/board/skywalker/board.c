/* SPDX-License-Identifier: GPL-2.0-only OR MIT */

#include <assert.h>
#include <libpayload.h>
#include "base/init_funcs.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/flash/mtk_snfc.h"
#include "drivers/gpio/mtk_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/storage/mtk_ufs.h"

static void enable_usb_vbus(struct UsbHostController *usb_host)
{
	/*
	 * To avoid USB detection issue, assert GPIO AP_XHCI_INIT_DONE
	 * to notify EC to enable USB VBUS when xHCI is initialized.
	 */
	GpioOps *pdn = sysinfo_lookup_gpio("XHCI init done", 1,
					   new_mtk_gpio_output);
	if (pdn) {
		gpio_set(pdn, 1);
		/* After USB VBUS is enabled, delay 500ms for USB detection. */
		mdelay(500);
	}
}

static int board_setup(void)
{
	sysinfo_install_flags(new_mtk_gpio_input);
	power_set_ops(&psci_power_ops);
	/* Set up NOR flash ops */
	MtkNorFlash *nor_flash = new_mtk_nor_flash(0x11018000);
	flash_set_ops(&nor_flash->ops);

	/* Set up UFS ops */
	MtkUfsCtlr *ufs_host = new_mtk_ufs_ctlr(0x112B0000, 0x112BD000);
	list_insert_after(&ufs_host->ufs.bctlr.list_node, &fixed_block_dev_controllers);

	/* TODO: Set up eMMC */
	/* TODO: Set up SD card */
	/* Set up USB */
	UsbHostController *usb_host = new_usb_hc(XHCI, 0x11260000);
	set_usb_init_callback(usb_host, enable_usb_vbus);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	return 0;
}

INIT_FUNC(board_setup);

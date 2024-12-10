/* SPDX-License-Identifier: GPL-2.0-only OR MIT */

#include <assert.h>
#include <libpayload.h>
#include "base/init_funcs.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/flash/mtk_snfc.h"
#include "drivers/gpio/mtk_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"

static int board_setup(void)
{
	sysinfo_install_flags(new_mtk_gpio_input);
	power_set_ops(&psci_power_ops);
	/* Set up NOR flash ops */
	MtkNorFlash *nor_flash = new_mtk_nor_flash(0x11018000);

	flash_set_ops(&nor_flash->ops);

	/* TODO: Set up eMMC */
	/* TODO: Set up SD card */
	/* Set up USB */
	UsbHostController *usb_host = new_usb_hc(XHCI, 0x11260000);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	return 0;
}

INIT_FUNC(board_setup);

// SPDX-License-Identifier: GPL-2.0

#include <assert.h>
#include <libpayload.h>
#include "base/init_funcs.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/flash/mtk_snfc.h"
#include "drivers/gpio/mtk_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/storage/mtk_mmc.h"
#include "drivers/video/display.h"
#include "drivers/video/mtk_ddp.h"

static int board_backlight_update(DisplayOps *me, uint8_t enable)
{
	GpioOps *pwm_control = sysinfo_lookup_gpio("PWM control", 1,
						   new_mtk_gpio_output);
	GpioOps *backlight_en = sysinfo_lookup_gpio("backlight enable", 1,
						    new_mtk_gpio_output);

	/* Enforce enable to be either 0 or 1. */
	enable = !!enable;

	/* TODO: Add eDP AUX for backlight control */
	if (pwm_control)
		gpio_set(pwm_control, enable);
	else
		printf("%s: Use eDP AUX to control panel backlight\n",
		       __func__);

	if (backlight_en)
		gpio_set(backlight_en, enable);

	return 0;
}

static int board_setup(void)
{
	sysinfo_install_flags(new_mtk_gpio_input);
	power_set_ops(&psci_power_ops);
	/* Set up NOR flash ops */
	MtkNorFlash *nor_flash = new_mtk_nor_flash(0x1132C000);
	flash_set_ops(&nor_flash->ops);

	/* Set up eMMC */
	MtkMmcTuneReg emmc_tune_reg = {
		.msdc_iocon = 0x1 << 8,
		.pad_tune = 0x10 << 16
	};
	MtkMmcHost *emmc = new_mtk_mmc_host(
		0x11230000, 0x11f50000, 400 * MHz, 200 * MHz, emmc_tune_reg,
		8, 0, NULL, MTK_MMC_V2);

	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	/* TODO: Set up SD card */

	/* Set up USB */
	UsbHostController *usb_host = new_usb_hc(XHCI, 0x11200000);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	/* Set display ops */
	if (display_init_required())
		display_set_ops(new_mtk_display(board_backlight_update,
						0x1C000000, 2));
	else
		printf("[%s] no display_init_required()!\n", __func__);

	return 0;
}

INIT_FUNC(board_setup);

/*
 * Copyright 2016 Rockchip Inc.
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

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "boot/fit.h"
#include "config.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/rockchip.h"
#include "drivers/gpio/sysinfo.h"
#include "vboot/util/flag.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/dw_mmc.h"
#include "drivers/storage/rk_dwmmc.h"
#include "drivers/flash/spi.h"
#include "drivers/bus/spi/rockchip.h"

static const int emmc_sd_clock_min = 400 * 1000;
static const int emmc_clock_max = 200 * 1000 * 1000;

static int board_setup(void)
{
	// Claim that we have an open lid to satisfy vboot.
	flag_replace(FLAG_LIDSW, new_gpio_high());

	// Claim that we have an power key to satisfy vboot.
	flag_replace(FLAG_PWRSW, new_gpio_low());

	RkSpi *spi1 = new_rockchip_spi(0xff1d0000);

	flash_set_ops(&new_spi_flash(&spi1->ops)->ops);

	SdhciHost *emmc = new_mem_sdhci_host((void *)0xfe330000,
					     SDHCI_PLATFORM_NO_EMMC_HS200 |
					     SDHCI_PLATFORM_NO_CLK_BASE,
					     emmc_sd_clock_min,
					     emmc_clock_max, 198);

	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	RkGpio *card_detect;

	 /* SDMMC_DET_L is different on different board revisions. */
	switch (lib_sysinfo.board_id) {
	case 0:  /* This is for Kevin rev1. */
		card_detect = new_rk_gpio_input(GPIO(4, D, 2));
		break;
	default:
		card_detect = new_rk_gpio_input(GPIO(4, D, 0));
		break;
	}

	GpioOps *card_detect_ops = &card_detect->ops;

	card_detect_ops = new_gpio_not(card_detect_ops);

	DwmciHost *sd_card = new_rkdwmci_host(0xfe320000, 594000000, 4, 1,
					      card_detect_ops);

	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	return 0;
}

INIT_FUNC(board_setup);

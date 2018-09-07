/*
 * Copyright 2018 Google Inc.
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
#include "drivers/bus/spi/mtk.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/mt8183.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/storage/mtk_mmc.h"
#include "vboot/util/flag.h"

static int board_setup(void)
{
	sysinfo_install_flags(new_mtk_gpio_input);
	flag_replace(FLAG_LIDSW, new_gpio_high());
	flag_replace(FLAG_PWRSW, new_gpio_low());

	power_set_ops(&psci_power_ops);

	MtkSpi *spi2 = new_mtk_spi(0x11012000);
	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spi2->ops);
	GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					      new_mtk_gpio_input);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_spi_bus->ops, 0, ec_int);
	register_vboot_ec(&cros_ec->vboot, 0);

	MtkSpi *spi1 = new_mtk_spi(0x11010000);
	flash_set_ops(&new_spi_flash(&spi1->ops)->ops);

	MtkMmcTuneReg emmc_tune_reg = {
		.msdc_iocon = 0x1 << 8,
		.pad_tune = 0x10 << 16
	};
	MtkMmcHost *emmc = new_mtk_mmc_host(
		0x11230000, 200 * MHz, 50 * MHz, emmc_tune_reg, 8, 0, NULL,
		MTK_MMC_V2);

	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	return 0;
}

INIT_FUNC(board_setup);

// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC
 *
 * Board file for Dedede.
 */

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/jasperlake.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/jasperlake.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"

/*
 * Clock frequencies for the eMMC and SD ports are defined below. The minimum
 * frequency is the same for both interfaces, the firmware does not run any
 * interface faster than 52 MHz, but defines maximum eMMC frequency as 200 MHz
 * for proper divider settings.
 */
#define EMMC_SD_CLOCK_MIN       400000
#define EMMC_CLOCK_MAX          200000000

static int board_setup(void)
{
	sysinfo_install_flags(new_jasperlake_gpio_input_from_coreboot);

	/* 32MB SPI Flash */
	flash_set_ops(&new_mem_mapped_flash(0xfe000000, 0x2000000)->ops);

	/* Chrome EC (eSPI) */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, 0, NULL);
	register_vboot_ec(&cros_ec->vboot, 0);

	/* PCH Power */
	power_set_ops(&jasperlake_power_ops);

	/* eMMC */
	SdhciHost *emmc = new_pci_sdhci_host((PCH_DEV_EMMC),
			SDHCI_PLATFORM_SUPPORTS_HS400ES,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	return 0;
}

INIT_FUNC(board_setup);

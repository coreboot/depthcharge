// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC
 *
 * Board file for Dedede.
 */

#include "base/init_funcs.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/jasperlake.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/jasperlake.h"

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

	return 0;
}

INIT_FUNC(board_setup);

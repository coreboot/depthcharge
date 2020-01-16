// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC
 *
 * Board file for Dedede.
 */

#include "base/init_funcs.h"
#include "drivers/gpio/jasperlake.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/jasperlake.h"

static int board_setup(void)
{
	sysinfo_install_flags(new_jasperlake_gpio_input_from_coreboot);

	/* PCH Power */
	power_set_ops(&jasperlake_power_ops);

	return 0;
}

INIT_FUNC(board_setup);

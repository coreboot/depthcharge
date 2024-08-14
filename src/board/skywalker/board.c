/* SPDX-License-Identifier: GPL-2.0-only OR MIT */

#include <assert.h>
#include <libpayload.h>
#include "base/init_funcs.h"
#include "drivers/flash/mtk_snfc.h"
#include "drivers/power/psci.h"

static int board_setup(void)
{
	power_set_ops(&psci_power_ops);
	/* Set up NOR flash ops */
	MtkNorFlash *nor_flash = new_mtk_nor_flash(0x11018000);

	flash_set_ops(&nor_flash->ops);

	/* TODO: Set up eMMC */
	/* TODO: Set up SD card */
	/* TODO: Set up USB */
	return 0;
}

INIT_FUNC(board_setup);

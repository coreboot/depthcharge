// SPDX-License-Identifier: GPL-2.0

#include "common.h"
#include "drivers/power/power.h"

static int do_reboot(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	reboot();
	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	reboot, 1, 1,
	"Reboot the system",
	NULL
);

static int do_poweroff(cmd_tbl_t *cmdtp, int flag, int argc,
	       char * const argv[])
{
	power_off();
	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	poweroff, 1, 1,
	"Power down the system",
	NULL
);

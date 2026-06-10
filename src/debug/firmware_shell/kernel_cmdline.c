// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright 2026 Google LLC
 *
 * Commands for modifying kernel command line.
 */

#include "common.h"
#include "boot/commandline.h"

static int do_cmdline_append(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;

	for (int i = 1; i < argc; i++)
		commandline_append(argv[i]);

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	cmdline_append,	256,	1,
	"Append to kernel command line",
	" param1 [param2 ...]"
);

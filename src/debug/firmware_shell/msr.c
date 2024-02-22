// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2024 Intel Corporation */

/*
 *
 * Commands for testing x86 MSR.
 */

#include "common.h"
#include <arch/msr.h>

static int do_rdmsr(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int size;
	u32 msr;

	if (argc < 2)
		return CMD_RET_USAGE;

	/* Check for size specification */
	size = cmd_get_data_size(argv[0], 1);
	if (size < 8)
		return CMD_RET_USAGE;

	/* Get MSR address */
	msr = strtoul(argv[1], NULL, 16);

	printf("0x%016llx\n", _rdmsr(msr));

	return CMD_RET_SUCCESS;
}

static int do_wrmsr(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int size;
	u64 value;
	u32 msr;

	if (argc < 3)
		return CMD_RET_USAGE;

	/* Check for size specification */
	size = cmd_get_data_size(argv[0], 1);
	if (size < 1)
		return CMD_RET_USAGE;

	/* Get MSR address */
	msr = strtoul(argv[1], NULL, 16);

	/* Get write value */
	value = strtoul(argv[2], NULL, 16);

	_wrmsr(msr, value);
	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	rdmsr,	2,	1,
	"MSR read",
	"[.q] msr"
);

U_BOOT_CMD(
	wrmsr,	3,	1,
	"MSR write",
	"[.q] msr value"
);

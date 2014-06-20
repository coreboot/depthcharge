/*
 * Copyright 2014 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this project.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but without
 * any warranty; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "debug/cli/common.h"
#include "vboot/boot.h"

static int do_boot(cmd_tbl_t *cmdtp, int flag,
		   int argc, char * const argv[])
{
	void *addr;

	if (argc < 2)
		return -1;

	addr = (void *) strtoul(argv[1], NULL, 16);

	return legacy_boot(addr, "dummy command");
}

U_BOOT_CMD(
	boot, CONFIG_SYS_MAXARGS, 1,
	"command for booting kernel, all shapes and kinds",
	" addr - address of the kernel blob in memory\n"
);

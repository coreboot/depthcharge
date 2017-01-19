/*
 * Copyright (C) 2017 Google Inc.
 * Copyright (C) 2017, 2018 Collabora Limited
 *
 * Author: Tomeu Vizoso <tomeu.vizoso@collabora.com>
 * Author: Guillaume Tucker <guillaume.tucker@collabora.com>
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

/*
 * Boot support
 */
#include "common.h"

#include "netboot/netboot.h"
#include "net/uiplib.h"

#define MAX_ARGS_LEN 4096

static void * const payload = (void *)(uintptr_t)CONFIG_KERNEL_START;

int do_tftpboot(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char *address;
	char *bootfile;
	char *argsfile;
	uip_ipaddr_t tftp_ip;

	if (argc != 4)
		return CMD_RET_USAGE;

	address = argv[1];
	bootfile = argv[2];
	argsfile = argv[3];

	if (!uiplib_ipaddrconv(address, &tftp_ip)) {
		printf("Invalid IPv4 address: %s\n", address);
		return CMD_RET_USAGE;
	}

	netboot(&tftp_ip, bootfile, argsfile, NULL);

	/* netboot() only returns if it failed */

	return CMD_RET_FAILURE;
}

U_BOOT_CMD(
	tftpboot,	4,	1,
	"boot image via network using TFTP protocol",
	"[host IP addr] [boot file] [args file]"
);

/*
 * Copyright 2014 Google Inc.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "netboot/netboot.h"
#include "netboot/params.h"
#include "net/uip.h"

/*
 * These are the real implementations for developer-build features that override
 * the symbols from stubs.c. They must never be linked into production images!
 */

void dc_dev_gdb_enter(void)
{
	gdb_enter();
}

void dc_dev_gdb_exit(int exit_code)
{
	gdb_exit(exit_code);
}

void dc_dev_netboot(void)
{
	uip_ipaddr_t *tftp_ip;
	char *bootfile, *argsfile;

	video_console_init();

	if (netboot_params_read(&tftp_ip, NULL, 0,
				&bootfile, &argsfile))
		printf("ERROR: Failed to read netboot parameters from flash\n");

	netboot(tftp_ip, bootfile, argsfile, NULL);
}

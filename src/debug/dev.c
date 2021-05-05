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
 */

#include <libpayload.h>

#include "netboot/netboot.h"
#include "netboot/params.h"
#include "net/uip.h"
#include "vboot/crossystem/crossystem.h"
#include "fastboot/fastboot.h"

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

	if (!CONFIG(HEADLESS))
		video_console_init();

	if (netboot_params_read(&tftp_ip, NULL, 0,
				&bootfile, &argsfile))
		printf("ERROR: Failed to read netboot parameters from flash\n");

	netboot(tftp_ip, bootfile, argsfile, NULL, NULL);
}

void dc_dev_fastboot(void)
{
	if (!CONFIG(HEADLESS))
		video_console_init();

	fastboot();

	// The only way to get here is via "fastboot continue". Drain any
	// pending characters (because the user probably spammed ctrl-f).
	while (havechar())
		getchar();

	if (!CONFIG(HEADLESS)) {
		console_remove_output_driver(video_console_putchar);
	}
}

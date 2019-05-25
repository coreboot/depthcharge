
/*
 * Copyright 2012 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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

#include "arch/sign_of_life.h"
#include "base/init_funcs.h"
#include "base/timestamp.h"
#include "boot/bcb.h"
#include "debug/cli/common.h"
#include "drivers/input/input.h"
#include "vboot/fastboot.h"
#include "vboot/stages.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/init_funcs.h"

void  __attribute__((weak)) vboot_try_fastboot(void)
{
	/* Default weak implementation. */
}

int main(void)
{
	// Let the world know we're alive.
	sign_of_life(0xaa);

	// Initialize some consoles.
	serial_console_init();
	cbmem_console_init();
	input_init();

	printf("\n\nStarting depthcharge on " CONFIG_BOARD "...\n");

	// Set up time keeping.
	timestamp_init();

	// Run any generic initialization functions that are compiled in.
	if (run_init_funcs())
		halt();

	timestamp_add_now(TS_RO_VB_INIT);

	// Set up common params, vboot_handoff, VBSD, and VbInit flags.
	if (common_params_init())
		halt();

	if (CONFIG_CLI)
		console_loop();

	/* Fastboot is only entered in recovery path */
	if (vboot_in_recovery())
		vboot_try_fastboot();

	/* Handle BCB command, if supported. */
	if (CONFIG_BCB_SUPPORT)
		bcb_handle_command();

	// Run any initialization functions before vboot takes control
	if (run_vboot_init_funcs())
		halt();

	timestamp_add_now(TS_VB_SELECT_AND_LOAD_KERNEL);

	// Select a kernel and boot it.
	if (vboot_select_and_load_kernel())
		halt();

	// We should never get here.
	printf("Got to the end!\n");
	halt();
	return 0;
}


/*
 * Copyright 2012 Google LLC
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
#include <vb2_api.h>

#include "arch/post_code.h"
#include "base/init_funcs.h"
#include "base/late_init_funcs.h"
#include "base/timestamp.h"
#include "debug/firmware_shell/common.h"
#include "drivers/input/input.h"
#include "vboot/stages.h"
#include "vboot/util/commonparams.h"

int main(void)
{
	// Let the world know we're alive.
	post_code(POST_CODE_START_DEPTHCHARGE);

	// Initialize some consoles.
	serial_console_init();
	cbmem_console_init();
	input_init();

	struct cb_mainboard *mainboard =
		phys_to_virt(lib_sysinfo.cb_mainboard);
	const char *mb_part_string = cb_mb_part_string(mainboard);
	printf("\n\nStarting depthcharge on %s...\n", mb_part_string);

	// Set up time keeping.
	timestamp_init();

	// Run any generic initialization functions that are compiled in.
	if (run_init_funcs())
		halt();

	timestamp_add_now(TS_RO_VB_INIT);

	// Wipe memory and enable USB if necessary (dev/rec mode). This *must*
	// happen before vboot init funcs to satisfy assumptions in AOA driver.
	vboot_check_wipe_memory();
	vboot_check_enable_usb();

	if (CONFIG(FIRMWARE_SHELL_ENTER_IMMEDIATELY))
		dc_dev_enter_firmware_shell();

	// Run any late initialization functions before vboot takes control.
	if (run_late_init_funcs())
		halt();

	timestamp_add_now(TS_VB_SELECT_AND_LOAD_KERNEL);

	// Select a kernel and boot it.
	if (vboot_select_and_boot_kernel())
		halt();

	// We should never get here.
	printf("Got to the end!\n");
	halt();
	return 0;
}

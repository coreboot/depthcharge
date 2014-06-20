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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "arch/sign_of_life.h"
#include "base/init_funcs.h"
#include "base/timestamp.h"
#include "config.h"
#include "debug/cli/common.h"
#include "drivers/input/input.h"
#include "vboot/stages.h"
#include "vboot/util/commonparams.h"

int main(void)
{
	// Let the world know we're alive.
	sign_of_life(0xaa);

	// Initialize some consoles.
	serial_console_init();
	cbmem_console_init();
	input_init();

	printf("\n\nStarting read-only depthcharge on " CONFIG_BOARD "...\n");

	// Set up time keeping.
	timestamp_init();

	// Run any generic initialization functions that are compiled in.
	if (run_init_funcs())
		halt();

	timestamp_add_now(TS_RO_PARAMS_INIT);

	if (CONFIG_CLI)
		console_loop();

	// Set up the common param structure, clearing shared data.
	if (common_params_init(1))
		halt();

	timestamp_add_now(TS_RO_VB_INIT);

	// Initialize vboot.
	if (vboot_init())
		halt();

	timestamp_add_now(TS_RO_VB_SELECT_FIRMWARE);

	// Select firmware.
	if (vboot_select_firmware())
		halt();

	timestamp_add_now(TS_RO_VB_SELECT_AND_LOAD_KERNEL);

	// Select a kernel and boot it.
	if (vboot_select_and_load_kernel())
		halt();

	// We should never get here.
	printf("Got to the end!\n");
	halt();
	return 0;
}

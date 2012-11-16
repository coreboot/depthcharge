/*
 * Copyright (c) 2012 The Chromium OS Authors.
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

#include "base/init_funcs.h"
#include "base/timestamp.h"
#include "drivers/ec/chromeos/mkbp.h"
#include "drivers/input/input.h"
#include "vboot/stages.h"
#include "vboot/util/acpi.h"
#include "vboot/util/commonparams.h"

int main(void)
{
	// Let the world know we're alive.
	outb(0xaa, 0x80);

	// Initialize some consoles.
	serial_init();
	cbmem_console_init();
	input_init();

	printf("\n\nStarting read-only depthcharge...\n");

	get_cpu_speed();
	timestamp_init();

	if (run_init_funcs())
		halt();

	// Set up the common param structure.
	if (common_params_init())
		halt();

	mkbp_init();
	if (mkbp_ptr) {
		// Unconditionally clear the EC recovery request.
		const uint32_t kb_rec_mask =
			EC_HOST_EVENT_MASK(EC_HOST_EVENT_KEYBOARD_RECOVERY);
		mkbp_clear_host_events(mkbp_ptr, kb_rec_mask);
	}

	// Initialize vboot.
	if (vboot_init())
		halt();

	// Select firmware.
	if (vboot_select_firmware())
		halt();

	if (acpi_update_data()) {
		printf("Failed to update ACPI data.\n");
		halt();
	}

	usb_initialize();
	// Select a kernel and boot it.

	if (vboot_select_and_load_kernel())
		halt();

	printf("Got to the end!\n");
	halt();
	return 0;
}

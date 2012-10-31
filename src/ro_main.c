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
#include <stdint.h>
#include <vboot_api.h>

#include "base/timestamp.h"
#include "drivers/ahci.h"
#include "drivers/ec/chromeos/mkbp.h"
#include "image/fmap.h"
#include "image/startrw.h"
#include "vboot/stages.h"
#include "vboot/util/acpi.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"

int main(void)
{
	// Let the world know we're alive.
	outb(0xaa, 0x80);

	// Initialize some consoles.
	serial_init();
	cbmem_console_init();

	printf("\n\nStarting read-only depthcharge...\n");

	get_cpu_speed();
	timestamp_init();

	if (fmap_init()) {
		printf("Problem with the FMAP.\n");
		halt();
	}

	// Set up the common param structure.
	if (common_params_init())
		halt();

	// Initialize vboot.
	int dev_switch = flag_fetch(FLAG_DEVSW);
	int rec_switch = flag_fetch(FLAG_RECSW);
	int wp_switch = flag_fetch(FLAG_WPSW);
	int oprom_loaded = flag_fetch(FLAG_OPROM);
	if (dev_switch < 0 || rec_switch < 0 ||
	    wp_switch < 0 || oprom_loaded < 0) {
		// An error message should have already been printed.
		halt();
	}

	if (vboot_init(dev_switch, rec_switch, wp_switch, oprom_loaded))
		halt();

	// Select firmware.
	mkbp_init();

	enum VbSelectFirmware_t select;
	if (vboot_select_firmware(&select))
		halt();

	// If an RW firmware was selected, start it.
	if (select == VB_SELECT_FIRMWARE_A || select == VB_SELECT_FIRMWARE_B) {
		FmapArea *rw_area;
		if (select == VB_SELECT_FIRMWARE_A)
			rw_area = fmap_find_area(main_fmap, "FW_MAIN_A");
		else
			rw_area = fmap_find_area(main_fmap, "FW_MAIN_B");
		uintptr_t rw_addr = rw_area->offset + main_rom_base;
		if (start_rw_firmware((void *)rw_addr))
			return 1;
	}

	int firmware_type = FIRMWARE_TYPE_NORMAL;
	if (select == VB_SELECT_FIRMWARE_RECOVERY)
		firmware_type = FIRMWARE_TYPE_RECOVERY;
	else if (dev_switch)
		firmware_type = FIRMWARE_TYPE_DEVELOPER;
	if (acpi_update_data(firmware_type)) {
		printf("Failed to update ACPI data.\n");
		halt();
	}

	// Select a kernel and boot it.

	// If we got this far and the option rom is loaded, we can assume
	// vboot wants to use the display and probably also wants to use the
	// keyboard.
	if (oprom_loaded) {
		keyboard_init();
		usb_initialize();
	}

	ahci_init(PCI_DEV(0, 31, 2));
	if (vboot_select_and_load_kernel())
		halt();

	printf("Got to the end!\n");
	halt();
	return 0;
}

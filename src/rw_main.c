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
#include <vboot_api.h>

#include "base/commonparams.h"
#include "base/dc_image.h"
#include "base/flag.h"
#include "base/fmap.h"
#include "base/memory.h"
#include "base/timestamp.h"
#include "boot/commandline.h"
#include "boot/zimage.h"
#include "drivers/ahci.h"
#include "drivers/ec/chromeos/mkbp.h"
#include "drivers/power_management.h"

#define CMD_LINE_SIZE 4096

static char cmd_line_buf[2 * CMD_LINE_SIZE];

static int vboot_select_and_load_kernel(void)
{
	static char cmd_line_temp[CMD_LINE_SIZE];

	VbSelectAndLoadKernelParams kparams = {
		.kernel_buffer = (void *)0x100000,
		.kernel_buffer_size = 0x800000
	};

	printf("Calling VbSelectAndLoadKernel().\n");
	if (VbSelectAndLoadKernel(&cparams, &kparams) != VBERROR_SUCCESS) {
		printf("Doing a cold reboot.\n");
		cold_reboot();
	}

	uintptr_t params_addr =
		kparams.bootloader_address - sizeof(struct boot_params);
	struct boot_params *params = (struct boot_params *)params_addr;
	uintptr_t cmd_line_addr = params_addr - CMD_LINE_SIZE;
	strncpy(cmd_line_temp, "cros_secure ", CMD_LINE_SIZE);
	strncat(cmd_line_temp, (char *)cmd_line_addr, CMD_LINE_SIZE);

	if (commandline_subst(cmd_line_temp, 0,
			      kparams.partition_number + 1,
			      kparams.partition_guid,
			      cmd_line_buf,
			      sizeof(cmd_line_buf)))
		return 1;

	if (zboot(params, cmd_line_buf, kparams.kernel_buffer))
		return 1;

	return 0;
}

int main(void)
{
	// Let the world know we're alive.
	outb(0xab, 0x80);

	// Initialize some consoles.
	serial_init();
	cbmem_console_init();

	printf("\n\nStarting read/write depthcharge...\n");

	// Initialize the video if available.
	video_init();
	video_console_cursor_enable(0);

	get_cpu_speed();
	timestamp_init();

	ahci_init(PCI_DEV(0, 31, 2));

	if (fmap_init()) {
		printf("Problem with the FMAP.\n");
		halt();
	}

	// The common params structure is already set up from the RO firmware.

	int oprom_loaded = flag_fetch(FLAG_OPROM);
	if (oprom_loaded < 0)
		halt();

	mkbp_init();

	// If we got this far and the option rom is loaded, we can assume
	// vboot wants to use the display and probably also wants to use the
	// keyboard.
	if (oprom_loaded)
		keyboard_init();

	if (vboot_select_and_load_kernel())
		halt();

	printf("Got to the end!\n");
	halt();
	return 0;
}

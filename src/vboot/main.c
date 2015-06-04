
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
#include <vboot_struct.h>

#include "arch/sign_of_life.h"
#include "base/init_funcs.h"
#include "base/timestamp.h"
#include "boot/bcb.h"
#include "config.h"
#include "debug/cli/common.h"
#include "drivers/input/input.h"
#include "vboot/fastboot.h"
#include "vboot/stages.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"
#include "vboot/util/vboot_handoff.h"

static int vboot_init_handoff()
{
	struct vboot_handoff *vboot_handoff;

	// Set up the common param structure, not clearing shared data.
	if (common_params_init(0))
		return 1;

	if (lib_sysinfo.vboot_handoff == NULL) {
		printf("vboot handoff pointer is NULL\n");
		return 1;
	}

	if (lib_sysinfo.vboot_handoff_size != sizeof(struct vboot_handoff)) {
		printf("Unexpected vboot handoff size: %d\n",
		       lib_sysinfo.vboot_handoff_size);
		return 1;
	}

	vboot_handoff = lib_sysinfo.vboot_handoff;

	/* If the lid is closed, don't count down the boot
	 * tries for updates, since the OS will shut down
	 * before it can register success.
	 *
	 * VbInit was already called in coreboot, so we need
	 * to update the vboot internal flags ourself.
	 */
	int lid_switch = flag_fetch(FLAG_LIDSW);
	if (!lid_switch) {
		VbSharedDataHeader *vdat;
		int vdat_size;

		if (find_common_params((void **)&vdat, &vdat_size) != 0)
			vdat = NULL;

		/* We need something to work with */
		if (vdat != NULL)
			/* Tell kernel selection to not count down */
			vdat->flags |= VBSD_NOFAIL_BOOT;
	}

	return vboot_do_init_out_flags(vboot_handoff->init_params.out_flags);
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

	if (CONFIG_CLI)
		console_loop();

	// Set up the common param structure, not clearing shared data.
	if (vboot_init_handoff())
		halt();

	vboot_try_fastboot();

	/* Handle BCB command, if supported. */
	if (CONFIG_BCB_SUPPORT)
		bcb_handle_command();

	timestamp_add_now(TS_VB_SELECT_AND_LOAD_KERNEL);

	// Select a kernel and boot it.
	if (vboot_select_and_load_kernel())
		halt();

	// We should never get here.
	printf("Got to the end!\n");
	halt();
	return 0;
}

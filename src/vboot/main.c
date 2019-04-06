
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
#include "vboot/util/init_funcs.h"
#include "vboot/util/vboot_handoff.h"

void  __attribute__((weak)) vboot_try_fastboot(void)
{
	/* Default weak implementation. */
}

static int vboot_verify_handoff(void)
{
	if (lib_sysinfo.vboot_handoff == NULL) {
		printf("vboot handoff pointer is NULL\n");
		return 1;
	}

	if (lib_sysinfo.vboot_handoff_size != sizeof(struct vboot_handoff)) {
		printf("Unexpected vboot handoff size: %d\n",
		       lib_sysinfo.vboot_handoff_size);
		return 1;
	}

	return 0;
}

static int vboot_update_shared_data(void)
{
	VbSharedDataHeader *vb_sd;
	int vb_sd_size;

	if (find_common_params((void **)&vb_sd, &vb_sd_size)) {
		printf("Unable to access VBSD\n");
		return 1;
	}

	/*
	 * If the lid is closed, kernel selection should not count down the
	 * boot tries for updates, since the OS will shut down before it can
	 * register success.
	 */
	if (!flag_fetch(FLAG_LIDSW))
		vb_sd->flags |= VBSD_NOFAIL_BOOT;

	if (flag_fetch(FLAG_WPSW))
		vb_sd->flags |= VBSD_BOOT_FIRMWARE_WP_ENABLED;

	if (CONFIG(EC_SOFTWARE_SYNC))
		vb_sd->flags |= VBSD_EC_SOFTWARE_SYNC;

	if (CONFIG(EC_SLOW_UPDATE))
		vb_sd->flags |= VBSD_EC_SLOW_UPDATE;

	if (CONFIG(EC_EFS))
		vb_sd->flags |= VBSD_EC_EFS;

	if (!CONFIG(PHYSICAL_REC_SWITCH))
		vb_sd->flags |= VBSD_BOOT_REC_SWITCH_VIRTUAL;

	return 0;
}

static int vboot_update_vbinit_flags(void)
{
	struct vboot_handoff *vboot_handoff = lib_sysinfo.vboot_handoff;
	/* VbInit was already called in coreboot, so we need to update the
	 * vboot internal flags ourself. */
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

	// Verify incoming vboot_handoff.
	if (vboot_verify_handoff())
		halt();

	// Set up the common param structure, not clearing shared data.
	if (common_params_init(0))
		halt();

	// Modify VbSharedDataHeader contents from vboot_handoff struct.
	if (vboot_update_shared_data())
		halt();

	// Retrieve data from VbInit flags.
	if (vboot_update_vbinit_flags())
		halt();

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

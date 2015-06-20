/*
 * Copyright 2015 Google Inc.
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

#include <assert.h>
#include <libpayload.h>
#include <vboot_nvstorage.h>
#include <vboot_struct.h>

#include "drivers/ec/cros/commands.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/power/power.h"
#include "fastboot/fastboot.h"
#include "vboot/stages.h"
#include "vboot/util/commonparams.h"

fb_ret_type __attribute__((weak)) device_mode_enter(void)
{
	return FB_CONTINUE_RECOVERY;
}

static void vboot_clear_recovery(void)
{
	vboot_update_recovery(VBNV_RECOVERY_NOT_REQUESTED);
}

static void vboot_set_recovery(void)
{
	vboot_update_recovery(VBNV_RECOVERY_FW_FASTBOOT);
}

enum {
	ENTRY_POINT_RECOVERY_MENU,
	ENTRY_POINT_FASTBOOT_MODE,
};

/*
 * Currently, all boards use EC to identify keyboard events. If in the future,
 * any other mechanism is used, every mechanism needs to have its own
 * implementation of this routine.
 */
uint8_t fastboot_keyboard_mask(void)
{
	uint32_t ec_events;
	const uint32_t kb_fastboot_mask =
		EC_HOST_EVENT_MASK(EC_HOST_EVENT_KEYBOARD_FASTBOOT);

	cros_ec_get_host_events(&ec_events);

	if (kb_fastboot_mask & ec_events) {
		cros_ec_clear_host_events(kb_fastboot_mask);
		return 1;
	}

	return 0;
}

uint8_t vboot_get_entrypoint(void)
{
	VbSharedDataHeader *vdat;
	int size;

	assert(find_common_params((void **)&vdat, &size) == 0);

	/*
	 * Enter fastboot mode when:
	 * 1. Recovery reason is set to fastboot requested by FW, or
	 * 2. Recovery reason is set to recovery requested by BCB, or
	 * 3. Recovery reason is set to manual request and keyboard mask is set
	 * to fastboot event.
	 *
	 * Else, enter recovery-menu to allow user to choose from different
	 * options.
	 */
	if ((vdat->recovery_reason == VBNV_RECOVERY_FW_FASTBOOT) ||
	    (vdat->recovery_reason == VBNV_RECOVERY_BCB_USER_MODE) ||
	    ((vdat->recovery_reason == VBNV_RECOVERY_RO_MANUAL) &&
	     fastboot_keyboard_mask()))
		return ENTRY_POINT_FASTBOOT_MODE;

	return ENTRY_POINT_RECOVERY_MENU;
}

void vboot_try_fastboot(void)
{
	if (vboot_in_recovery() == 0)
		return;

	if (board_should_enter_device_mode() == 0)
		return;

	uint8_t entry_point = vboot_get_entrypoint();

	/* TBD(furquan/pgeorgi): Add recovery menu */
	if (entry_point == ENTRY_POINT_RECOVERY_MENU)
		printf("RECOVERY MENU NOT YET SUPPORTED!\n");

	vboot_clear_recovery();

	fb_ret_type ret = device_mode_enter();

	switch(ret) {
	case FB_REBOOT:
		/* Does not return */
		cold_reboot();
		break;
	case FB_REBOOT_BOOTLOADER:
		vboot_set_recovery();
		/* Does not return */
		cold_reboot();
		break;
	case FB_POWEROFF:
		/* Does not return */
		power_off();
		break;
	default:
		/* Continue in recovery mode */
		break;
	}
}

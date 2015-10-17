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
#include <keycodes.h>
#include <vboot_nvstorage.h>
#include <vboot_struct.h>

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

static int is_fastboot_mode_requested(void)
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
	return ((vdat->recovery_reason == VBNV_RECOVERY_FW_FASTBOOT) ||
	    (vdat->recovery_reason == VBNV_RECOVERY_BCB_USER_MODE) ||
	    ((vdat->recovery_reason == VBNV_RECOVERY_RO_MANUAL) &&
	     fb_board_handler.keyboard_mask &&
	     fb_board_handler.keyboard_mask()));
}

static void udc_fastboot(const char *reason)
{
	if (fb_board_handler.enter_device_mode == NULL) {
		printf("ERROR: No handler for entering device mode\n");
		return;
	}

	if (fb_board_handler.enter_device_mode() == 0) {
		printf("Board does not want to enter device mode\n");
		return;
	}

	printf("Entering fastboot mode: %s\n", reason);

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
	case FB_CONTINUE_RECOVERY:
		/* Continue in cros recovery mode */
		break;
	default:
		/* unknown error. reboot into fastboot menu */
		vboot_set_recovery();
		cold_reboot();
		break;
	}
}

void vboot_try_fastboot(void)
{
	const char *fb_mode_reason = NULL;

	/*
	 * Enter fastboot mode:
	 * 1. If it is requested explicitly through recovery reason, or
	 * 2. If device does not support menu in recovery mode, or
	 * 3. If device supports menu and fastboot mode is requested through
	 * menu.
	 */
	if (is_fastboot_mode_requested())
		fb_mode_reason = "Requested by user";
	else if (fb_board_handler.enter_menu == NULL)
		fb_mode_reason = "No menu support";
	else if (fb_board_handler.enter_menu())
		fb_mode_reason = "Selected from menu";

	if (fb_mode_reason)
		udc_fastboot(fb_mode_reason);
}

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

#include <libpayload.h>
#include <vboot_nvstorage.h>
#include <vboot_api.h>

#include "drivers/power/power.h"
#include "fastboot/fastboot.h"

fb_ret_type __attribute__((weak)) device_mode_enter(void)
{
	return FB_CONTINUE_RECOVERY;
}

static void vboot_update_recovery(uint32_t request)
{
	VbNvContext context;

	VbExNvStorageRead(context.raw);
	VbNvSetup(&context);

	VbNvSet(&context, VBNV_RECOVERY_REQUEST, request);

	VbNvTeardown(&context);
	if (context.raw_changed)
		VbExNvStorageWrite(context.raw);

}

static void vboot_clear_recovery(void)
{
	vboot_update_recovery(VBNV_RECOVERY_NOT_REQUESTED);
}

static void vboot_set_recovery(void)
{
	vboot_update_recovery(VBNV_RECOVERY_RO_MANUAL);
}

void vboot_try_fastboot(int is_recovery)
{
	if (is_recovery == 0)
		return;

	if (board_should_enter_device_mode() == 0)
		return;

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

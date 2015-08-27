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
#include <gbb_header.h>
#include <vboot_api.h>
#include <vboot_nvstorage.h>

#include "fastboot/capabilities.h"
#include "vboot/stages.h"
#include "vboot/util/commonparams.h"
#include "vboot/vbnv.h"

enum {
	FB_LIMITED_CAP = 0,
	FB_FULL_CAP    = 1,
};

static uint32_t fb_cap_bitmap[] = {
	/* Limited fastboot functionality allowed in firmware. */
	[FB_LIMITED_CAP] = (FB_ID_GETVAR | FB_ID_DOWNLOAD | FB_ID_BOOT |
			    FB_ID_CONTINUE | FB_ID_REBOOT |
			    FB_ID_REBOOT_BOOTLOADER | FB_ID_POWERDOWN |
			    FB_ID_BATTERY_CUTOFF | FB_ID_LOCK | FB_ID_UNLOCK |
			    FB_ID_OFF_MODE_CHARGE | FB_ID_GET_UNLOCK_ABILITY),
	/* Full fastboot functionality allowed in firmware. */
	[FB_FULL_CAP] = FB_ID_MASK,
};

uint8_t fb_check_gbb_override(void)
{
	GoogleBinaryBlockHeader *gbb = cparams.gbb_data;
	if (memcmp(gbb->signature, GBB_SIGNATURE, GBB_SIGNATURE_SIZE)) {
		printf("Bad signature on GBB.\n");
		return 0;
	}

	if (gbb->flags & GBB_FLAG_FORCE_DEV_BOOT_FASTBOOT_FULL_CAP)
		return 1;

	return 0;
}

int __attribute__((weak)) board_allow_unlock(void)
{
	/* Default weak implemenation, always returns 0 */
	return 0;
}

static uint32_t fb_get_curr_cap_bitmap(void)
{
	static uint32_t bitmap = 0;

	if (bitmap)
		return bitmap;

	/* If GBB flag is set to allow full fastboot capability, so do we. */
	if (fb_check_gbb_override()) {
		bitmap = fb_cap_bitmap[FB_FULL_CAP];
		return bitmap;
	}

	/* If we are currently in normal mode, only limited cap is available */
	if (vboot_in_developer() == 0) {

		bitmap = fb_cap_bitmap[FB_LIMITED_CAP];

		return bitmap;
	}

	/*
	 * If we are in developer mode, we need to read the fastboot_full_cap
	 * flag from nvstorage.
	 * If the flag is set, full fastboot capability is enabled in firmware.
	 */
	if (vbnv_read(VBNV_DEV_BOOT_FASTBOOT_FULL_CAP))
		bitmap = fb_cap_bitmap[FB_FULL_CAP];
	else
		bitmap = fb_cap_bitmap[FB_LIMITED_CAP];

	return bitmap;
}

fb_cap_status_t fb_cap_func_allowed(fb_func_id_t id)
{
	uint32_t cap_bitmap = fb_get_curr_cap_bitmap();

	if (cap_bitmap & id)
		return FB_CAP_FUNC_ALLOWED;

	return FB_CAP_FUNC_NOT_ALLOWED;
}

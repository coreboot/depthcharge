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

#include "image/fmap.h"

VbError_t VbExHashFirmwareBody(VbCommonParams *cparams,
			       uint32_t firmware_index)
{
	const char *area_name = NULL;

	switch (firmware_index) {
	case VB_SELECT_FIRMWARE_A:
		area_name = "FW_MAIN_A";
		break;
	case VB_SELECT_FIRMWARE_B:
		area_name = "FW_MAIN_B";
		break;
	default:
		printf("Unrecognized firmware index %d.\n", firmware_index);
		return VBERROR_UNKNOWN;
	}

	FmapArea *area = fmap_find_area(main_fmap, area_name);
	if (!area) {
		printf("Fmap region %s not found.\n", area_name);
		return VBERROR_UNKNOWN;
	}

	void *data = (void *)((uintptr_t)area->area_offset + main_rom_base);
	VbUpdateFirmwareBodyHash(cparams, data, area->area_size);

	return VBERROR_SUCCESS;
}

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

#include <assert.h>
#include <libpayload.h>
#include <vboot_api.h>

#include "config.h"
#include "drivers/flash/flash.h"
#include "image/fmap.h"
#include "image/index.h"

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

	FmapArea area;
	if (fmap_find_area(area_name, &area)) {
		printf("Fmap region %s not found.\n", area_name);
		return VBERROR_UNKNOWN;
	}

	void *data;
	uint32_t size;
	if (CONFIG_EC_SOFTWARE_SYNC) {
		/*
		 * If EC software sync is enabled, the EC RW hash and the
		 * system RW are bundled together with a small header in the
		 * front. This figures out how big the header and data are
		 * so we hash the right amount of stuff.
		 */

		const SectionIndex *index = index_from_fmap(&area);
		if (!index)
			return VBERROR_UNKNOWN;

		size = sizeof(SectionIndex) +
			index->count * sizeof(SectionIndexEntry);
		for (int i = 0; i < index->count; i++)
			size += (index->entries[i].size + 3) & ~3;

		if (area.size < size) {
			printf("Bad RW index size.\n");
			return VBERROR_UNKNOWN;
		}
		data = flash_read(area.offset, size);
	} else {
		data = flash_read(area.offset, area.size);
		size = area.size;
	}

	VbUpdateFirmwareBodyHash(cparams, data, size);

	return VBERROR_SUCCESS;
}

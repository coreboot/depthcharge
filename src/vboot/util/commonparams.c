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

#include "image/fmap.h"
#include "image/symbols.h"
#include "vboot/util/acpi.h"
#include "vboot/util/commonparams.h"

VbCommonParams cparams CPARAMS;
uint8_t shared_data_blob[VB_SHARED_DATA_REC_SIZE] SHARED_DATA;

int common_params_init(void)
{
	// Set up the common param structure.
	memset(&cparams, 0, sizeof(cparams));
	memset(shared_data_blob, 0, sizeof(shared_data_blob));

	FmapArea *gbb_area = fmap_find_area(main_fmap, "GBB");
	if (!gbb_area) {
		printf("Couldn't find the GBB.\n");
		return 1;
	}

	cparams.gbb_data =
		(void *)(uintptr_t)(gbb_area->offset + main_rom_base);
	cparams.gbb_size = gbb_area->size;

	chromeos_acpi_t *acpi_table = (chromeos_acpi_t *)lib_sysinfo.vdat_addr;
	cparams.shared_data_blob = (void *)&acpi_table->vdat;
	cparams.shared_data_size = sizeof(acpi_table->vdat);
	return 0;
}

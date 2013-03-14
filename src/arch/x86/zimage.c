/*
 * Copyright 2013 Google Inc.
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "arch/x86/boot.h"
#include "vboot/boot.h"
#include "vboot/util/acpi.h"

int boot(void *kernel, char *cmd_line, void *params, void *loader)
{
	// If nobody's prepared the boot_params structure for us already,
	// do that now.
	if (!params) {
		const int SectSize = 512;

		struct boot_params *bparams = (struct boot_params *)kernel;

		// Find the kernel header.
		struct setup_header *header = &bparams->hdr;
		uintptr_t header_start = (uintptr_t)header;
		uintptr_t header_end = (uintptr_t)&header->jump +
			((header->jump >> 8) & 0xff);
		uintptr_t header_size = header_end - header_start;

		// Prepare the boot params (zeropage).
		static struct boot_params tmp_params;
		memset(&tmp_params, 0, sizeof(tmp_params));
		memcpy(&tmp_params.hdr, header, header_size);
		params = &tmp_params;

		// Move the protected mode part of the kernel into place.
		uintptr_t pm_offset = (header->setup_sects + 1) * SectSize;
		uintptr_t pm_size = header->syssize * 16;
		uintptr_t pm_start = (uintptr_t)kernel + pm_offset;
		memmove(kernel, (void *)pm_start, pm_size);
	}

	return boot_x86_linux(params, cmd_line, kernel);
}

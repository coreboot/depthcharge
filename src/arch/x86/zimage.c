/*
 * Copyright 2013 Google LLC
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
 */

#include <libpayload.h>

#include "arch/x86/boot.h"
#include "vboot/boot.h"
#include "vboot/boot_info.h"
#include "vboot/util/acpi.h"

/* See "The Linux/x86 Boot Protocol" in kernel docs */
#define INITRD_MAX_ADDRESS 0x37FFFFFF

int boot(struct boot_info *bi)
{
	/*
	 * If boot_info has vboot kernel params and flags in the kernel params
	 * indicate that the image is not Chrome OS kernel image, bail out
	 * early.
	 */
	if (bi->kparams &&
	    (GET_KERNEL_IMG_TYPE(bi->kparams->flags) != KERNEL_IMAGE_CROS)) {
		printf("zimage: Not a Chrome OS kernel image\n");
		return -1;
	}

	// If nobody's prepared the boot_params structure for us already,
	// do that now.
	if (!bi->params) {
		const int SectSize = 512;

		struct boot_params *bparams = (struct boot_params *)bi->kernel;

		// Find the kernel header.
		struct setup_header *header = &bparams->hdr;

		// Add ramdisk if provided
		if (bi->ramdisk_addr) {
			uint32_t initrd_addr_max = header->initrd_addr_max;

			if (initrd_addr_max > INITRD_MAX_ADDRESS)
				initrd_addr_max = INITRD_MAX_ADDRESS;

			void *ramdisk = (void*)ALIGN_DOWN(
				initrd_addr_max - bi->ramdisk_size, 4096);

			unsigned long kernel_size = header->syssize * 16;

			if (((uintptr_t)bi->kernel + kernel_size) >
			    (uintptr_t)ramdisk) {
				printf("Not enough space for initrd\n");
				return -1;
			}

			memcpy(ramdisk, bi->ramdisk_addr, bi->ramdisk_size);
			header->ramdisk_image = (uintptr_t)ramdisk;
			header->ramdisk_size = bi->ramdisk_size;
		}

		uintptr_t header_start = (uintptr_t)header;
		uintptr_t header_end = (uintptr_t)&header->jump +
			((header->jump >> 8) & 0xff);
		uintptr_t header_size = header_end - header_start;

		// Prepare the boot params (zeropage).
		static struct boot_params tmp_params;
		memset(&tmp_params, 0, sizeof(tmp_params));
		memcpy(&tmp_params.hdr, header, header_size);
		bi->params = &tmp_params;

		// Move the protected mode part of the kernel into place.
		uintptr_t pm_offset = (header->setup_sects + 1) * SectSize;
		uintptr_t pm_size = header->syssize * 16;
		uintptr_t pm_start = (uintptr_t)bi->kernel + pm_offset;
		memmove(bi->kernel, (void *)pm_start, pm_size);
	}

	return boot_x86_linux(bi->params, bi->cmd_line, bi->kernel);
}

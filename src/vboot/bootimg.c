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
#include <stdint.h>

#include "vboot/boot.h"

#define BOOTIMG_MAGIC			"ANDROID!"
#define BOOTIMG_MAGIC_SIZE		8
#define BOOTIMG_NAME_SIZE		16
#define BOOTIMG_ARGS_SIZE		512
#define BOOTIMG_EXTRA_ARGS_SIZE	1024
#define BOOTIMG_ID_SIZE		8

struct bootimg_hdr {
	uint8_t magic[BOOTIMG_MAGIC_SIZE];
	uint32_t kernel_size;
	uint32_t kernel_addr;
	uint32_t ramdisk_size;
	uint32_t ramdisk_addr;
	uint32_t second_size;
	uint32_t second_addr;
	uint32_t tags_addr;
	uint32_t page_size;
	uint32_t unused[2];
	uint8_t name[BOOTIMG_NAME_SIZE];
	uint8_t cmdline[BOOTIMG_ARGS_SIZE];
	uint32_t id[BOOTIMG_ID_SIZE];
	uint8_t extra_cmdline[BOOTIMG_EXTRA_ARGS_SIZE];
};

int bootimg_get_info(struct boot_info *bi, void *addr)
{
	struct bootimg_hdr *hdr = addr;
	uintptr_t kernel;
	uintptr_t ramdisk_addr;
	uint32_t kernel_size;

	if (memcmp(hdr->magic, BOOTIMG_MAGIC, BOOTIMG_MAGIC_SIZE)) {
		printf("Oops no match for bootimg\n");
		return 1;
	}

	kernel = (uintptr_t)hdr + hdr->page_size;
	bi->kernel = (void *)kernel;

	bi->cmd_line = (char *)hdr->cmdline;

	/*
	 * kernel_size should be aligned up to page_size since ramdisk always
	 * starts in a new page. The align up operation is performed on
	 * kernel_size rather than (kernel + hdr->kernel_size) because hdr might
	 * not be on an aligned boundary itself and thus aligning (kernel +
	 * hdr->kernel_size) can erroneously make ramdisk_addr point to an
	 * address beyond the ramdisk start.
	 * Aligning kernel_size to hdr->page_size gives us the size of kernel in
	 * multiple of pages.
	 */
	kernel_size = ALIGN_UP(hdr->kernel_size, hdr->page_size);
	ramdisk_addr = kernel + kernel_size;

	bi->ramdisk_size = hdr->ramdisk_size;
	bi->ramdisk_addr = (void *)ramdisk_addr;

	return 0;
}

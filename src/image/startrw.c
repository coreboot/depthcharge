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

#include <libpayload.h>
#include <lzma.h>

#include "base/elf.h"
#include "image/enter_trampoline.h"
#include "image/startrw.h"
#include "image/symbols.h"

int start_rw_firmware(const void *compressed_image, uint32_t size)
{
	// Put the decompressed RW ELF at the end of the trampoline.
	void *elf_image = (void *)&_tramp_end;

	// Decompress the RW image.
	uint32_t out_size = ulzman(compressed_image, size, elf_image,
				   (uint8_t *)&_kernel_end -
				   (uint8_t *)&_tramp_end);
	if (!out_size) {
		printf("Error decompressing RW firmware.\n");
		return -1;
	}

	// Check that it's a reasonable ELF image.
	unsigned char *e_ident = elf_image;
	if (e_ident[0] != ElfMag0Val || e_ident[1] != ElfMag1Val ||
		e_ident[2] != ElfMag2Val || e_ident[3] != ElfMag3Val) {
		printf("Bad ELF magic value in RW firmware.\n");
		return -1;
	}
	if (e_ident[EI_Class] != ElfClass32) {
		printf("Only loading of 32 bit RW firmware is supported.\n");
		return -1;
	}

	enter_trampoline((Elf32_Ehdr *)elf_image);

	// We should never actually reach the end of this function.
	return 0;
}

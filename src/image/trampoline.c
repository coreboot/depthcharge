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

#include <string.h>

#include "base/elf.h"
#include "image/symbols.h"
#include "image/trampoline.h"

uint8_t trampoline_stack[16384] __attribute__((aligned(16)));

void ENTRY trampoline(Elf32_Ehdr *ehdr)
{
	void *new_stack = trampoline_stack + sizeof(trampoline_stack) -
		sizeof(uint32_t);
	__asm__ __volatile__(
		"mov %[new_stack], %%esp\n"
		"call real_trampoline\n"
		:: [new_stack]"r"(new_stack), [ehdr]"a"(ehdr)
		: "memory"
	);
}

void real_trampoline(Elf32_Ehdr *ehdr)
{
	uintptr_t base = (uintptr_t)ehdr;
	uintptr_t addr = (uintptr_t)ehdr + ehdr->e_phoff;
	uintptr_t step = ehdr->e_phentsize;
	int num = ehdr->e_phnum;

	// Copy over the ELF segments.
	while (num--) {
		Elf32_Phdr *phdr = (Elf32_Phdr *)addr;
		addr += step;

		if (phdr->p_type != ElfPTypeLoad)
			continue;

		uint8_t *dest = (uint8_t *)(uintptr_t)phdr->p_paddr;
		uint8_t *src = (uint8_t *)(uintptr_t)(base + phdr->p_offset);
		uint32_t filesz = phdr->p_filesz;
		uint32_t memsz = phdr->p_memsz;

		if (filesz)
			memcpy(dest, src, filesz);
		if (memsz > filesz)
			memset(dest + filesz, 0, memsz - filesz);
	}

	// Go for it!
	typedef void (*entry_func)(void);
	((entry_func)ehdr->e_entry)();
}

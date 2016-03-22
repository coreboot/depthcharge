/*
 * Copyright 2014 Google Inc.
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
 */

#include <stdint.h>

#include "base/elf.h"
#include "image/symbols.h"
#include "image/enter_trampoline.h"

void enter_trampoline(Elf32_Ehdr *ehdr)
{
	/* put here MIPS equivalent of the below. */
#if 0
	__asm__ __volatile__(
		"mov sp, %[new_stack]\n"
		"mov r0, %[ehdr]\n"
		"mov r1, %[cb_header_ptr]\n"
		// Need register addressing since the call goes too far
		"bx %[load_elf]\n"
		::[new_stack]"r"(&_tramp_estack - 8), [ehdr]"r"(ehdr),
		 [zero]"r"(0), [load_elf]"r"(&tramp_load_elf),
		 [cb_header_ptr]"r"(cb_header_ptr)
		: "memory", "r0", "r1", "sp"
	);
#endif
}

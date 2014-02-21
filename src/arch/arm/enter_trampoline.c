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
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <stdint.h>

#include "base/elf.h"
#include "image/symbols.h"
#include "image/enter_trampoline.h"

extern void *cb_header_ptr;

void enter_trampoline(Elf32_Ehdr *ehdr)
{
	__asm__ __volatile__(
		"mov sp, %[new_stack]\n"
		"mov r0, %[ehdr]\n"
		"mov r1, %[cb_header_ptr]\n"
		// Need register addressing since the call goes too far
		"bx %[load_elf]\n"
		:: [new_stack]"r"(&_tramp_estack - 8), [ehdr]"r"(ehdr),
		   [zero]"r"(0), [load_elf]"r"(&tramp_load_elf),
		   [cb_header_ptr]"r"(cb_header_ptr)
		: "memory", "r0", "r1", "sp"
	);
}

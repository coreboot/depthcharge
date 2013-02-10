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

#ifndef __BASE_ELF_H__
#define __BASE_ELF_H__

#include <stdint.h>

/* ELF file header. */

typedef enum {
	EI_Mag0 = 0,
	EI_Mag1 = 1,
	EI_Mag2 = 2,
	EI_Mag3 = 3,
	EI_Class = 4,
	EI_Data = 5,
	EI_Version = 6,
	EI_OsAbi = 7,
	EI_AbiVersion = 8,
	EI_Pad = 9,
	EI_NIdent = 16
} ElfIdentIndex;

typedef enum {
	ElfMag0Val = 0x7f,
	ElfMag1Val = 'E',
	ElfMag2Val = 'L',
	ElfMag3Val = 'F'
} ElfMagVal;

typedef enum {
	ElfClassNone = 0,
	ElfClass32 = 1,
	ElfClass64 = 2
} ElfClassVal;

typedef struct {
	unsigned char e_ident[EI_NIdent];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint32_t e_entry;
	uint32_t e_phoff;
	uint32_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
} Elf32_Ehdr;

typedef struct {
	unsigned char e_ident[EI_NIdent];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
} Elf64_Ehdr;


/* ELF program header. */

typedef enum {
	ElfPTypeNull = 0,
	ElfPTypeLoad = 1,
	ElfPTypeDynamic = 2,
	ElfPTypeInterp = 3,
	ElfPTypeNote = 4,
	ElfPTypeShlib = 5,
	ElfPTypePhdr = 6,
	ElfPTypeTls = 7,
	ElfPTypeLoOs = 0x60000000,
	ElfPTypeHiOs = 0x6fffffff,
	ElfPTypeLoProc = 0x70000000,
	ElfPTypeHiProc = 0x7fffffff
} ElfPTypeVal;

typedef struct {
	uint32_t p_type;
	uint32_t p_offset;
	uint32_t p_vaddr;
	uint32_t p_paddr;
	uint32_t p_filesz;
	uint32_t p_memsz;
	uint32_t p_flags;
	uint32_t p_align;
} Elf32_Phdr;

typedef struct {
	uint32_t p_type;
	uint32_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_flags;
	uint64_t p_align;
} Elf64_Phdr;

#endif /* __BASE_ELF_H__ */

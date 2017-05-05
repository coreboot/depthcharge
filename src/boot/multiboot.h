/*
 * Copyright (C) 1999,2003,2007,2008,2009,2010  Free Software Foundation, Inc.
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

#ifndef __BOOT_MULTIBOOT_H__
#define __BOOT_MULTIBOOT_H__

#include <stdint.h>

/* How many bytes from the start of the file we search for the header.  */
#define MULTIBOOT_SEARCH			8192
#define MULTIBOOT_HEADER_ALIGN			4

/* The magic field should contain this.  */
#define MULTIBOOT_HEADER_MAGIC			0x1BADB002

/* This should be in %eax.  */
#define MULTIBOOT_BOOTLOADER_MAGIC		0x2BADB002

/* Alignment of multiboot modules.  */
#define MULTIBOOT_MOD_ALIGN			0x00001000

/* Alignment of the multiboot info structure.  */
#define MULTIBOOT_INFO_ALIGN			0x00000004

/* Flags set in the 'flags' member of the multiboot header.  */

/* Align all boot modules on i386 page (4KB) boundaries.  */
#define MULTIBOOT_PAGE_ALIGN			0x00000001

/* Must pass memory information to OS.  */
#define MULTIBOOT_MEMORY_INFO			0x00000002

/* Must pass video information to OS.  */
#define MULTIBOOT_VIDEO_MODE			0x00000004

/* This flag indicates the use of the address fields in the header.  */
#define MULTIBOOT_AOUT_KLUDGE			0x00010000

/* Flags to be set in the 'flags' member of the multiboot info structure.  */

/* is there basic lower/upper memory information? */
#define MULTIBOOT_INFO_MEMORY			0x00000001
/* is there a boot device set? */
#define MULTIBOOT_INFO_BOOTDEV			0x00000002
/* is the command-line defined? */
#define MULTIBOOT_INFO_CMDLINE			0x00000004
/* are there modules to do something with? */
#define MULTIBOOT_INFO_MODS			0x00000008

/* These next two are mutually exclusive */

/* is there a symbol table loaded? */
#define MULTIBOOT_INFO_AOUT_SYMS		0x00000010
/* is there an ELF section header table? */
#define MULTIBOOT_INFO_ELF_SHDR			0X00000020

/* is there a full memory map? */
#define MULTIBOOT_INFO_MEM_MAP			0x00000040

#ifndef ASM_FILE

struct multiboot_header
{
	/* Must be MULTIBOOT_MAGIC - see above.  */
	uint32_t magic;

	/* Feature flags.  */
	uint32_t flags;

	/* The above fields plus this one must equal 0 mod 2^32. */
	uint32_t checksum;

	/* These are only valid if MULTIBOOT_AOUT_KLUDGE is set.  */
	uint32_t header_addr;
	uint32_t load_addr;
	uint32_t load_end_addr;
	uint32_t bss_end_addr;
	uint32_t entry_addr;

	/* These are only valid if MULTIBOOT_VIDEO_MODE is set.  */
	uint32_t mode_type;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
};

/* The symbol table for a.out.  */
struct multiboot_aout_symbol_table
{
	uint32_t tabsize;
	uint32_t strsize;
	uint32_t addr;
	uint32_t reserved;
};

/* The section header table for ELF.  */
struct multiboot_elf_section_header_table
{
	uint32_t num;
	uint32_t size;
	uint32_t addr;
	uint32_t shndx;
};

struct multiboot_info
{
	/* Multiboot info version number */
	uint32_t flags;

	/* Available memory from BIOS */
	uint32_t mem_lower;
	uint32_t mem_upper;

	/* "root" partition */
	uint32_t boot_device;

	/* Kernel command line */
	uint32_t cmdline;

	/* Boot-Module list */
	uint32_t mods_count;
	uint32_t mods_addr;

	union
	{
		struct multiboot_aout_symbol_table aout_sym;
		struct multiboot_elf_section_header_table elf_sec;
	} u;

	/* Memory Mapping buffer */
	uint32_t mmap_length;
	uint32_t mmap_addr;
};

struct multiboot_mmap_entry
{
	uint32_t size;
	uint64_t addr;
	uint64_t len;
#define MULTIBOOT_MEMORY_AVAILABLE		1
#define MULTIBOOT_MEMORY_RESERVED		2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE       3
#define MULTIBOOT_MEMORY_NVS                    4
#define MULTIBOOT_MEMORY_BADRAM                 5
	uint32_t type;
} __attribute__((packed));

struct multiboot_mod_list
{
	/* memory used goes from bytes 'mod_start' to 'mod_end-1' inclusive */
	uint32_t mod_start;
	uint32_t mod_end;

	/* Module command line */
	uint32_t cmdline;

	/* padding to take it to 16 bytes (must be zero) */
	uint32_t pad;
};

struct boot_info;
int multiboot_fill_boot_info(struct boot_info *bi);
int multiboot_boot(struct boot_info *bi);

#endif /* ! ASM_FILE */

#endif /* ! __BOOT_MULTIBOOT__ */

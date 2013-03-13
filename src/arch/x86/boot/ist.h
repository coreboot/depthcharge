#ifndef __ARCH_X86_BOOT_IST_H__
#define __ARCH_X86_BOOT_IST_H__

/*
 * Include file for the interface to IST BIOS
 * Copyright 2002 Andy Grover <andrew.grover@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */


#include <stdint.h>

struct ist_info {
	uint32_t signature;
	uint32_t command;
	uint32_t event;
	uint32_t perf_level;
};

#endif /* __ARCH_X86_BOOT_IST_H__ */

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

#include "arch/arm/boot.h"
#include "base/cleanup_funcs.h"
#include "base/timestamp.h"
#include "vboot/boot.h"

static inline uint32_t get_sctlr(void)
{
	uint32_t val;
	asm("mrc p15, 0, %0, c1, c0, 0" : "=r" (val));
	return val;
}

static inline void set_sctlr(uint32_t val)
{
	asm volatile("mcr p15, 0, %0, c1, c0, 0" :: "r" (val));
	asm volatile("" ::: "memory");
}

void boot_arm_linux_jump(void *entry, uint32_t machine_type, void *fdt)
	__attribute__((noreturn));

int boot_arm_linux(uint32_t machine_type, void *fdt, void *entry)
{
	run_cleanup_funcs(CleanupOnHandoff);

	static const uint32_t SctlrM = (0x1 << 0);
	static const uint32_t SctlrC = (0x1 << 2);

	uint32_t sctlr = get_sctlr();

	timestamp_add_now(TS_START_KERNEL);

	// Flush dcache and icache to make loaded code visible.
	cache_sync_instructions();

	// Turn off the MMU.
	sctlr &= ~SctlrM;

	// Disable the data/unified cache.
	sctlr &= ~SctlrC;

	set_sctlr(sctlr);

	tlb_invalidate_all();

	boot_arm_linux_jump(entry, machine_type, fdt);

	return 0;
}

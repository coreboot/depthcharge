/*
 * Copyright (C) 2018 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_SOC_COMMON_PCR_H__
#define __DRIVERS_SOC_COMMON_PCR_H__

#include <stdint.h>

/* Port Id lives in bits 23:16 and register offset lives in 15:0 of address. */
#define PCH_PCR_PORTID_SHIFT	16

static inline void *pcr_port_regs(uintptr_t base, uint8_t pid)
{
	uintptr_t reg_addr;

	/* Create an address based off of port id and offset. */
	reg_addr = base;
	reg_addr += ((uintptr_t)pid) << PCH_PCR_PORTID_SHIFT;

	return (void *)reg_addr;
}

#endif /* __DRIVERS_SOC_COMMON_PCR_H__ */


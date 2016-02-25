/*
 * Copyright (C) 2016 Google Inc.
 * Copyright (C) 2015 Intel Corporation.
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

#ifndef __DRIVERS_SOC_SKYLAKE_H__
#define __DRIVERS_SOC_SKYLAKE_H__

#include <stdint.h>

/* Skylake PCR access to GPIO registers. */
#define PCH_PCR_BASE_ADDRESS	0xfd000000

/* Port Id lives in bits 23:16 and register offset lives in 15:0 of address. */
#define PCH_PCR_PORTID_SHIFT	16

/* PCR PIDs. */
#define PCH_PCR_PID_GPIOCOM3	0xAC
#define PCH_PCR_PID_GPIOCOM2	0xAD
#define PCH_PCR_PID_GPIOCOM1	0xAE
#define PCH_PCR_PID_GPIOCOM0	0xAF
#define PCH_PCR_PID_ITSS	0xC4

static inline void *pcr_port_regs(u8 pid)
{
	uintptr_t reg_addr;

	/* Create an address based off of port id and offset. */
	reg_addr = PCH_PCR_BASE_ADDRESS;
	reg_addr += ((uintptr_t)pid) << PCH_PCR_PORTID_SHIFT;

	return (void *)reg_addr;
}

#endif /* __DRIVERS_SOC_SKYLAKE_H__ */

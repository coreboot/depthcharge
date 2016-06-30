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

#ifndef __DRIVERS_SOC_APOLLOLAKE_H__
#define __DRIVERS_SOC_APOLLOLAKE_H__

#include <stdint.h>

/* Apollolake PCR access to GPIO registers. */
#define PCH_PCR_BASE_ADDRESS 0xd0000000

/* Port Id lives in bit 23:16 and register offset lives in 15:0 of address */
#define PCH_PCR_PORTID_SHIFT	16

/* PCR PIDs. */
#define PCH_PCR_PID_GPIO_SOUTHWEST	0xc0
#define PCH_PCR_PID_GPIO_SOUTH		0xc2
#define PCH_PCR_PID_GPIO_NORTHWEST	0xc4
#define PCH_PCR_PID_GPIO_NORTH		0xc5
#define PCH_PCR_PID_GPIO_WEST		0xc7

void *pcr_port_regs(uint8_t pid);

#endif /* __DRIVERS_SOC_APOLLOLAKE_H__ */

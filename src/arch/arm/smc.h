/*
 * Copyright 2016 Google Inc.
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
 */

#ifndef __ARCH_ARM_SMC_H__
#define __ARCH_ARM_SMC_H__

#include <stdint.h>

// From ARM PSCI specification (ARM DEN 0022C). Expand as needed.
enum psci_function_id {
	PSCI_SYSTEM_OFF = 0x84000008,
	PSCI_SYSTEM_RESET = 0x84000009,
};

// Conforms to ARM SMC Calling Convention (ARM DEN 0028A).
uint64_t smc(uint64_t function_id, uint64_t arg1, uint64_t arg2, uint64_t arg3);

#endif /* __ARCH_ARM_SMC_H__ */

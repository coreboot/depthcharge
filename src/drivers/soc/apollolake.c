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

#include "drivers/soc/apollolake.h"

void *pcr_port_regs(uint8_t pid)
{
	uintptr_t reg_addr;

	/* Create an address based off of port id and offset. */
	reg_addr = PCH_PCR_BASE_ADDRESS;
	reg_addr += ((uintptr_t)pid) << PCH_PCR_PORTID_SHIFT;

	return (void *)reg_addr;
}

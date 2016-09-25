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

#include <libpayload.h>

#include "drivers/soc/apollolake.h"

void *pcr_port_regs(uint8_t pid)
{
	uintptr_t reg_addr;

	/* Create an address based off of port id and offset. */
	reg_addr = PCH_PCR_BASE_ADDRESS;
	reg_addr += ((uintptr_t)pid) << PCH_PCR_PORTID_SHIFT;

	return (void *)reg_addr;
}

int apollolake_get_gpe(int gpe)
{
	int bank;
	uint32_t mask, sts;

	if (gpe < 0 || gpe > GPE0_DW3_31)
		return -1;

	bank = gpe / 32;
	mask = 1 << (gpe % 32);

	sts = inl(ACPI_PMIO_BASE + GPE0_STS(bank));
	if (sts & mask) {
		outl(mask, ACPI_PMIO_BASE + GPE0_STS(bank));
		return 1;
	}
	return 0;
}

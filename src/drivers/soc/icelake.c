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

#include <arch/io.h>
#include <libpayload.h>
#include <stdint.h>
#include "drivers/soc/common/iomap.h"
#include "drivers/soc/icelake.h"

int icelake_get_gpe(int gpe)
{
	int bank;
	uint32_t mask, sts;
	uint64_t start;
	int rc = 0;
	const uint64_t timeout_us = 1000;

	if (gpe < 0 || gpe > GPE_MAX)
		return -1;

	bank = gpe / 32;
	mask = 1 << (gpe % 32);

	/* Wait for GPE status to clear */
	start = timer_us(0);
	do {
		if (timer_us(start) > timeout_us)
			break;

		sts = inl(ACPI_BASE_ADDRESS + GPE0_STS(bank));
		if (sts & mask) {
			outl(mask, ACPI_BASE_ADDRESS + GPE0_STS(bank));
			rc = 1;
		}
	} while (sts & mask);

	return rc;
}

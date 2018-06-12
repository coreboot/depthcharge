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

int apollolake_get_gpe(int gpe)
{
	int bank;
	uint32_t mask, sts;
	uint64_t start;
	int rc = 0;
	const uint64_t timeout_us = 1000;

	if (gpe < 0 || gpe > GPE0_DW3_31)
		return -1;

	bank = gpe / 32;
	mask = 1 << (gpe % 32);

	/* Wait for GPE status to clear */
	start = timer_us(0);
	do {
		if (timer_us(start) > timeout_us)
			break;

		sts = inl(ACPI_PMIO_BASE + GPE0_STS(bank));
		if (sts & mask) {
			outl(mask, ACPI_PMIO_BASE + GPE0_STS(bank));
			rc = 1;
		}
	} while (sts & mask);

	return rc;
}

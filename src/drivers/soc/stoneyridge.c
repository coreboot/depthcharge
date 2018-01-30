/*
 * Copyright (C) 2017 Google Inc.
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
#include <arch/msr.h>
#include <libpayload.h>
#include <stdint.h>

#include "drivers/soc/stoneyridge.h"

int stoneyridge_get_gpe(int gpe)
{
	uint32_t mask, sts;
	uint64_t start;
	int rc = 0;
	const uint64_t timeout_us = 1000;

	if (gpe < 0 || gpe > 31)
		return -1;

	mask = 1 << gpe;

	/* Wait for GPE status to clear */
	start = timer_us(0);
	do {
		if (timer_us(start) > timeout_us)
			break;

		sts = readl(EVENT_STATUS);
		if (sts & mask) {
			writel(mask, EVENT_STATUS);
			rc = 1;
		}
	} while (sts & mask);

	return rc;
}

#define CU_PTSC_MSR	0xc0010280
#define PTSC_FREQ_MHZ	100

uint64_t timer_hz(void)
{
	return 1 * MHz;
}

uint64_t timer_raw_value(void)
{
	return _rdmsr(CU_PTSC_MSR) / PTSC_FREQ_MHZ;
}

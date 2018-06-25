/*
 * Copyright (C) 2016 Google Inc.
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

#include "base/cleanup_funcs.h"
#include "base/init_funcs.h"
#include "drivers/soc/common/iomap.h"
#include "drivers/soc/skylake.h"

static int pit_8254_enable(struct CleanupFunc *cleanup, CleanupType type)
{
	/* Unconditionally enable the 8254 in the Legacy path. */
	const uint32_t cge8254_mask_off = ~(1 << 2);
	const int itssprc_offset = 0x3300;
	void *itssprc = pcr_port_regs(PCH_PCR_PID_ITSS) + itssprc_offset;
	uint32_t reg = read32(itssprc);

	reg &= cge8254_mask_off;
	write32(itssprc, reg);

	return 0;
}

static int pit_8254_cleanup_install(void)
{
	static CleanupFunc dev =
	{
		&pit_8254_enable,
		CleanupOnLegacy,
		NULL
	};

	list_insert_after(&dev.list_node, &cleanup_funcs);
	return 0;
}

INIT_FUNC(pit_8254_cleanup_install);

int skylake_get_gpe(int gpe)
{
	int bank;
	uint32_t mask, sts;
	uint64_t start;
	int rc = 0;
	const uint64_t timeout_us = 1000;

	if (gpe < 0 || gpe > GPE0_WADT)
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

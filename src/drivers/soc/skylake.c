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

#include <libpayload.h>
#include <stdint.h>

#include "base/cleanup_funcs.h"
#include "base/init_funcs.h"
#include "drivers/soc/common/iomap.h"
#include "drivers/soc/common/pcr.h"
#include "drivers/soc/skylake.h"
#include "drivers/soc/intel_common.h"

static const SocGpeConfig cfg = {
	.gpe_max = GPE_MAX,
	.acpi_base = ACPI_BASE_ADDRESS,
	.gpe0_sts_off = GPE0_STS_OFF,
};

static int pit_8254_enable(struct CleanupFunc *cleanup, CleanupType type)
{
	/* Unconditionally enable the 8254 in the Legacy path. */
	const uint32_t cge8254_mask_off = ~(1 << 2);
	const int itssprc_offset = 0x3300;
	void *itssprc = pcr_port_regs(PCH_PCR_BASE_ADDRESS, PCH_PCR_PID_ITSS)
			+ itssprc_offset;
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
	return soc_get_gpe(gpe, &cfg);
}

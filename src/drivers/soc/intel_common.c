// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 *
 * Common SoC driver for Intel SOC.
 */

#include <arch/io.h>
#include <libpayload.h>
#include <stdint.h>
#include "drivers/soc/common/iomap.h"
#include "drivers/soc/intel_common.h"

#define GPE0_STS_SIZE 4
#define GPE0_STS(off, x) ((off) + ((x) * GPE0_STS_SIZE))

int soc_get_gpe(int gpe, const SocGpeConfig *cfg)
{
	int bank;
	uint32_t mask, sts;
	uint64_t start;
	int rc = 0;
	const uint64_t timeout_us = 1000;
	uint16_t gpe0_sts_port;

	if (gpe < 0 || gpe > cfg->gpe_max)
		return -1;

	bank = gpe / 32;
	mask = 1 << (gpe % 32);
	gpe0_sts_port = cfg->acpi_base + GPE0_STS(cfg->gpe0_sts_off, bank);

	/* Wait for GPE status to clear */
	start = timer_us(0);
	do {
		if (timer_us(start) > timeout_us)
			break;

		sts = inl(gpe0_sts_port);
		if (sts & mask) {
			outl(mask, gpe0_sts_port);
			rc = 1;
		}
	} while (sts & mask);

	return rc;
}

// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Intel Corporation.
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

#include "base/init_funcs.h"
#include "drivers/soc/alderlake.h"
#include "drivers/soc/common/iomap.h"
#include "drivers/soc/intel_common.h"
#include "drivers/bus/usb/intel_tcss.h"

#define CPU_PCIE_RP_SLOT_0          1
#define CPU_PCIE_RP_SLOT_1          6
#define PCH_PCIE_RP_SLOT_0          0x1c
#define PCH_PCIE_RP_SLOT_1          0x1d

static const SocGpeConfig cfg = {
	.gpe_max = GPE_MAX,
	.acpi_base = ACPI_BASE_ADDRESS,
	.gpe0_sts_off = GPE0_STS_OFF,
};

int alderlake_get_gpe(int gpe)
{
	return soc_get_gpe(gpe, &cfg);
}

static const TcssCtrlr adl_tcss_ctrlr = {
	.regbar = 0xfb000000,
	.iom_pid = 0xc1,
	.iom_status_offset = 0x160
};

const SocPcieRpGroup adl_cpu_rp_groups[] = {
	{ .slot = 0x1, .first_fn = 0, .count = 1 },
	{ .slot = 0x6, .first_fn = 0, .count = 1 },
	{ .slot = 0x6, .first_fn = 2, .count = 1 },
};
const unsigned int adl_cpu_rp_groups_count = ARRAY_SIZE(adl_cpu_rp_groups);

const SocPcieRpGroup adl_pch_rp_groups[] = {
	{ .slot = 0x1c, .first_fn = 0, .count = 8 },
	{ .slot = 0x1d, .first_fn = 0, .count = 4 },
};
const unsigned int adl_pch_rp_groups_count = ARRAY_SIZE(adl_pch_rp_groups);

const SocPcieRpGroup *soc_get_rp_group(pcidev_t dev, size_t *count)
{
	switch (PCI_SLOT(dev)) {
	case CPU_PCIE_RP_SLOT_0: /* fallthrough */
	case CPU_PCIE_RP_SLOT_1:
		*count = adl_cpu_rp_groups_count;
		return adl_cpu_rp_groups;

	case PCH_PCIE_RP_SLOT_0: /* fallthrough */
	case PCH_PCIE_RP_SLOT_1:
		*count = adl_pch_rp_groups_count;
		return adl_pch_rp_groups;

	default:
		*count = 0;
		return NULL;
	}
}

static int register_tcss(void)
{
	if (CONFIG(DRIVER_USB_INTEL_TCSS))
		register_tcss_ctrlr(&adl_tcss_ctrlr);

	return 0;
}

INIT_FUNC(register_tcss);

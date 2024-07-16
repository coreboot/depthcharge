// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2024 Intel Corporation */

#include <libpayload.h>

#include "drivers/bus/pci/pci.h"
#include "drivers/soc/common/iomap.h"
#include "drivers/soc/intel_common.h"
#include "drivers/soc/pantherlake.h"

#define PCIE_RP_SLOT_0  		0x1c
#define PCIE_RP_SLOT_1  		0x06

static const SocGpeConfig cfg = {
	.gpe_max = GPE_MAX,
	.acpi_base = ACPI_BASE_ADDRESS,
	.gpe0_sts_off = GPE0_STS_OFF,
};

int pantherlake_get_gpe(int gpe)
{
	return soc_get_gpe(gpe, &cfg);
}

static const SocPcieRpGroup ptl_rp_groups[] = {
	{ .slot = PCIE_RP_SLOT_0, .first_fn = 0, .count = 8 },
	{ .slot = PCIE_RP_SLOT_1, .first_fn = 0, .count = 4 },
};
const unsigned int ptl_rp_groups_count = ARRAY_SIZE(ptl_rp_groups);

const SocPcieRpGroup *soc_get_rp_group(pcidev_t dev, size_t *count)
{
	switch (PCI_SLOT(dev)) {
	case PCIE_RP_SLOT_0: /* fallthrough */
	case PCIE_RP_SLOT_1:
		*count = ptl_rp_groups_count;
		return ptl_rp_groups;

	default:
		*count = 0;
		return NULL;
	}
}

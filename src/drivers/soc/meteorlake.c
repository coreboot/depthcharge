// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2022 Intel Corporation */

#include "drivers/soc/meteorlake.h"
#include "drivers/soc/common/iomap.h"
#include "drivers/soc/intel_common.h"

#define PCIE_RP_SLOT_0	0x1c
#define PCIE_RP_SLOT_1	0x06

static const SocGpeConfig cfg = {
	.gpe_max = GPE_MAX,
	.acpi_base = ACPI_BASE_ADDRESS,
	.gpe0_sts_off = GPE0_STS_OFF,
};

int meteorlake_get_gpe(int gpe)
{
	return soc_get_gpe(gpe, &cfg);
}

const SocPcieRpGroup mtl_rp_groups[] = {
	{ .slot = 0x1c, .first_fn = 0, .count = 8 },
	{ .slot = 0x06, .first_fn = 0, .count = 3 },
};
const unsigned int mtl_rp_groups_count = ARRAY_SIZE(mtl_rp_groups);

const SocPcieRpGroup *soc_get_rp_group(pcidev_t dev, size_t *count)
{
	switch (PCI_SLOT(dev)) {
	case PCIE_RP_SLOT_0: /* fallthrough */
	case PCIE_RP_SLOT_1:
		*count = mtl_rp_groups_count;
		return mtl_rp_groups;

	default:
		*count = 0;
		return NULL;
	}
}

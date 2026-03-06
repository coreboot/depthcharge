// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "drivers/bus/pci/pci.h"
#include "drivers/soc/common/iomap.h"
#include "drivers/soc/intel_common.h"
#include "drivers/soc/novalake.h"

#define PCIE_RP_SLOT_0	0x1c
#define PCIE_RP_SLOT_1	0x06

static const SocGpeConfig cfg = {
	.gpe_max = GPE_MAX,
	.acpi_base = ACPI_BASE_ADDRESS,
	.gpe0_sts_off = GPE0_STS_OFF,
};

int novalake_get_gpe(int gpe)
{
	return soc_get_gpe(gpe, &cfg);
}

static const SocPcieRpGroup nvl_rp_groups[] = {
	{ .slot = PCIE_RP_SLOT_0, .first_fn = 0, .count = 8 },
	{ .slot = PCIE_RP_SLOT_1, .first_fn = 0, .count = 4 },
};

const unsigned int nvl_rp_groups_count = ARRAY_SIZE(nvl_rp_groups);

const SocPcieRpGroup *soc_get_rp_group(pcidev_t dev, size_t *count)
{
	switch (PCI_SLOT(dev)) {
	case PCIE_RP_SLOT_0: /* fallthrough */
	case PCIE_RP_SLOT_1:
		*count = nvl_rp_groups_count;
		return nvl_rp_groups;

	default:
		*count = 0;
		return NULL;
	}
}

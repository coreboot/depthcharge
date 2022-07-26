// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2022 Intel Corporation */

#include <libpayload.h>

#include "drivers/bus/pci/pci.h"
#include "drivers/bus/usb/intel_tcss.h"
#include "drivers/soc/common/iomap.h"
#include "drivers/soc/intel_common.h"
#include "drivers/soc/meteorlake.h"

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

static const SocPcieRpGroup mtl_rp_groups[] = {
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

void *port_status_reg(int port)
{
	uintptr_t tcss_xhci_bar;
	uintptr_t status_reg;

        pcidev_t dev = PCI_DEV(0, PCI_DEV_SLOT_TCSS, 0);

        if (get_pci_bar(dev, &tcss_xhci_bar)) {
                printf("Failed to get BAR for TCSS XHCI %d.%d.%d", PCI_BUS(dev),
                        PCI_SLOT(dev), PCI_FUNC(dev));
                return NULL;
        }

	/* tcss_xhci_bar + USB3_PORTSC_OFFSET + SSPort#0x10[0] */
	status_reg = tcss_xhci_bar + USB3_PORTSC_OFFSET + 0x10*port;
	return (void *)status_reg;
}

bool is_port_connected(int port)
{
	uint32_t sts;
	uintptr_t *status_reg = port_status_reg(port);

	/*
	 * Meteor Lake needs to allocate 256M for IOE die which cannot be fitted within 4G.
	 * IOE_PCR_ABOVE_4G_BASE_ADDRESS is defined as 0x3FFF0000000. IOM access is restricted
	 * because of the following reasons:
	 * 1. 32bit depthcharge cannot access the 64bit IOE regbar.
	 * 2. IOE P2SB access is blocked after the platform initialization by FSP.
	 * It is defined to read Type-C xhci host status register.
	 */
	if (!status_reg)
		return 0;

	sts = read32(status_reg);
	return !!(sts & USB3_PORT_STATUS_CONNECTED);
}

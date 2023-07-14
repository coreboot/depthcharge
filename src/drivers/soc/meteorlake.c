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

static void *port_status_reg(int port, int xhci)
{
	uintptr_t xhci_bar;
	uintptr_t status_reg;
	uint32_t offset = 0;
	pcidev_t dev = PCI_DEV(0, xhci, 0);

	if (xhci == PCI_DEV_SLOT_TCSS_XHCI)
		offset = USB3_PORTSC_OFFSET;
	else if (xhci == PCI_DEV_SLOT_PCH_XHCI)
		offset = USB2_PORTSC_OFFSET;
	else
		return NULL;

        if (get_pci_bar(dev, &xhci_bar)) {
                printf("Failed to get BAR for TCSS XHCI %d.%d.%d", PCI_BUS(dev),
                        PCI_SLOT(dev), PCI_FUNC(dev));
                return NULL;
        }

	/* xhci_bar + USB_PORTSC_OFFSET + Port#0x10[0] */
	status_reg = xhci_bar + offset + 0x10*port;
	return (void *)status_reg;
}

static bool usb_check_port_sc(uintptr_t *reg)
{
	return (read32(reg) & USB_PORT_STATUS_CONNECTED);
}

bool is_port_connected(int port, int usb2)
{
	uintptr_t *usb2_reg = port_status_reg(usb2, PCI_DEV_SLOT_PCH_XHCI);
	uintptr_t *usb3_reg = port_status_reg(port, PCI_DEV_SLOT_TCSS_XHCI);

	/*
	 * Meteor Lake needs to allocate 256M for IOE die which cannot be fitted within 4G.
	 * IOE_PCR_ABOVE_4G_BASE_ADDRESS is defined as 0x3FFF0000000. IOM access is restricted
	 * because of the following reasons:
	 * 1. 32bit depthcharge cannot access the 64bit IOE regbar.
	 * 2. IOE P2SB access is blocked after the platform initialization by FSP.
	 * 3. Type-C version servo V4 has USB2 and USB3 hubs. The servo V4 does not work on
	 *    USB3 + CCD mode and only works as USB2 hub on Type-C port 0.
	 * is_port_connected() function is defined with inputs port and usb2 where port is
	 * referred to TCSS USB3 and usb2 towards PCH USB2 ports number. We check usb2 and usb3
	 * status registers to determine the port connection.
	*/

	return (usb_check_port_sc(usb2_reg) || usb_check_port_sc(usb3_reg));
}

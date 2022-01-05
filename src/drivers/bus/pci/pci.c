// SPDX-License-Identifier: GPL-2.0
/* Copyright 2022 Google LLC.  */

#include <libpayload.h>
#include <pci/pci.h>

#include "drivers/bus/pci/pci.h"

int is_pci_bridge(pcidev_t dev)
{
	uint8_t header_type = pci_read_config8(dev, REG_HEADER_TYPE);
	header_type &= 0x7f;
	return header_type == HEADER_TYPE_BRIDGE;
}

/* Discover the register file address of the PCI device. */
int get_pci_bar(pcidev_t dev, uintptr_t *bar)
{
	uint32_t addr;

	addr = pci_read_config32(dev, PCI_BASE_ADDRESS_0);

	if (addr == ((uint32_t)~0))
		return -1;

	*bar = (uintptr_t)(addr & PCI_BASE_ADDRESS_MEM_MASK);

	return 0;
}

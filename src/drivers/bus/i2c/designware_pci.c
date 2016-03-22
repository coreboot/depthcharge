/*
 * Copyright (C) 2015 Google Inc.
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
#include <pci/pci.h>

#include "drivers/bus/i2c/designware.h"

/*
 * new_pci_designware_i2c - Allocate new i2c bus on PCI device.
 *
 * @dev:	PCI device for I2C controller
 * @speed:	required i2c speed
 *
 * Allocate new designware i2c bus.
 */
DesignwareI2c *new_pci_designware_i2c(pcidev_t dev, int speed)
{
	uint32_t addr = pci_read_config32(dev, PCI_BASE_ADDRESS_0);

	if (addr == ((uint32_t)~0)) {
		printf("%s: Error: PCI I2C @ %02x:%02x.%01x not found\n",
		       __func__, PCI_BUS(dev), PCI_SLOT(dev), PCI_FUNC(dev));
		return NULL;
	}
	addr &= ~0xf;

	return new_designware_i2c((uintptr_t)addr, speed);
}

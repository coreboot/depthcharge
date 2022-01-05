/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2022 Google LLC.  */

#ifndef __DRIVERS_BUS_PCI_H__
#define __DRIVERS_BUS_PCI_H__

#include <libpayload.h>

int is_pci_bridge(pcidev_t dev);

int get_pci_bar(pcidev_t dev, uintptr_t *bar);

#endif /* __DRIVERS_BUS_PCI_H__ */

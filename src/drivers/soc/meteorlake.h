// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2022 Intel Corporation */

#ifndef __DRIVERS_SOC_METEORLAKE_H__
#define __DRIVERS_SOC_METEORLAKE_H__

#include <stdint.h>
#include "drivers/soc/common/intel_gpe.h"
#include "drivers/soc/intel_common.h"

#define PCI_DEV_SATA	PCI_DEV(0, 0x17, 0)
#define PCI_DEV_PCIE0	PCI_DEV(0, 0x1c, 0)
#define PCI_DEV_PCIE2	PCI_DEV(0, 0x1c, 2)
#define PCI_DEV_PCIE4	PCI_DEV(0, 0x1c, 4)
#define PCI_DEV_PCIE5	PCI_DEV(0, 0x1c, 5)
#define PCI_DEV_PCIE6	PCI_DEV(0, 0x1c, 6)
#define PCI_DEV_PCIE7	PCI_DEV(0, 0x1c, 7)
#define PCI_DEV_PCIE8	PCI_DEV(0, 0x06, 0)
#define PCI_DEV_PCIE9	PCI_DEV(0, 0x06, 1)
#define PCI_DEV_PCIE10	PCI_DEV(0, 0x06, 2)

extern const SocPcieRpGroup *soc_get_rp_group(pcidev_t dev, size_t *count);


/* GPE */
#define GPE0_STS_OFF		0x60
extern int meteorlake_get_gpe(int gpe);

#endif /* __DRIVERS_SOC_METEORLAKE_H__ */

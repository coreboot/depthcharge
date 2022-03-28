// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2022 Intel Corporation */

#ifndef __DRIVERS_SOC_METEORLAKE_H__
#define __DRIVERS_SOC_METEORLAKE_H__

#include <stdint.h>
#include "drivers/soc/common/intel_gpe.h"

#define PCI_DEV_SATA	PCI_DEV(0, 0x17, 0)
#define PCI_DEV_PCIE0	PCI_DEV(0, 0x1c, 0)
#define PCI_DEV_PCIE8	PCI_DEV(0, 0x06, 0)
#define PCI_DEV_PCIE9	PCI_DEV(0, 0x06, 1)
#define PCI_DEV_PCIE10	PCI_DEV(0, 0x06, 2)

/* GPE */
#define GPE0_STS_OFF		0x60
extern int meteorlake_get_gpe(int gpe);

#endif /* __DRIVERS_SOC_METEORLAKE_H__ */

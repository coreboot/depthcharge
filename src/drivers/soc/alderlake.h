// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Intel Corporation.
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

#ifndef __DRIVERS_SOC_ALDERLAKE_H__
#define __DRIVERS_SOC_ALDERLAKE_H__

#include <stdint.h>
#include "drivers/soc/common/intel_gpe.h"
#include "drivers/soc/intel_common.h"

#define PCH_DEV_SATA	PCI_DEV(0, 0x17, 0)
#define SA_DEV_CPU_RP0	PCI_DEV(0, 0x06, 0)
#define PCH_DEV_EMMC	PCI_DEV(0, 0x1a, 0)
#define PCH_DEV_PCIE0	PCI_DEV(0, 0x1c, 0)
#define PCH_DEV_PCIE2	PCI_DEV(0, 0x1c, 2)
#define PCH_DEV_PCIE4	PCI_DEV(0, 0x1c, 4)
#define PCH_DEV_PCIE5	PCI_DEV(0, 0x1c, 5)
#define PCH_DEV_PCIE6	PCI_DEV(0, 0x1c, 6)
#define PCH_DEV_PCIE7	PCI_DEV(0, 0x1c, 7)
#define PCH_DEV_PCIE8	PCI_DEV(0, 0x1d, 0)
#define PCH_DEV_PCIE11  PCI_DEV(0, 0x1d, 3)
#define PCH_DEV_GSPI0	PCI_DEV(0, 0x1e, 2)
#define PCH_DEV_GSPI1	PCI_DEV(0, 0x1e, 3)
#define PCH_DEV_UFS1	PCI_DEV(0, 0x12, 7)

/* PCR Interface */
#define PCH_PCR_BASE_ADDRESS	0xfd000000
#define PCH_PCR_PID_GPIOCOM0	0x6e
#define PCH_PCR_PID_GPIOCOM1	0x6d
#define PCH_PCR_PID_GPIOCOM2	0x6c
#define PCH_PCR_PID_GPIOCOM3	0x6b
#define PCH_PCR_PID_GPIOCOM4	0x6a
#define PCH_PCR_PID_GPIOCOM5	0x69

/* I2C Designware Controller runs at 133MHz */
#define ALDERLAKE_DW_I2C_MHZ	133

extern const SocPcieRpGroup adl_cpu_rp_groups[];
extern const unsigned int adl_cpu_rp_groups_count;

extern const SocPcieRpGroup adl_pch_rp_groups[];
extern const unsigned int adl_pch_rp_groups_count;

extern const SocPcieRpGroup *soc_get_rp_group(pcidev_t dev, size_t *count);

/* GPE definitions */

int alderlake_get_gpe(int gpe);

#define GPE0_STS_OFF		0x60

#define REGBAR_PID_SHIFT		16
#define IOM_PORT_STATUS_CONNECTED	BIT(31)

#endif /* __DRIVERS_SOC_ALDERLAKE_H__ */

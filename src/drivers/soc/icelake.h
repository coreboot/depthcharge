/*
 * Copyright (C) 2018 Intel Corporation.
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

#ifndef __DRIVERS_SOC_ICELAKE_H__
#define __DRIVERS_SOC_ICELAKE_H__

#include <stdint.h>
#include "drivers/soc/common/intel_gpe.h"

#define PCH_DEV_SATA	PCI_DEV(0, 0x17, 0)
#define PCH_DEV_EMMC	PCI_DEV(0, 0x1a, 0
#define PCH_DEV_SDCARD	PCI_DEV(0, 0x14, 5)
#define PCH_DEV_PCIE9	PCI_DEV(0, 0x1d, 0)

/* Icelake PCR access to GPIO registers. */
#define PCH_PCR_BASE_ADDRESS	0xfd000000

#define GPE0_STS_OFF             0x60


/* PCR PIDs. */
#define PCH_PCR_PID_GPIOCOM5	0x69
#define PCH_PCR_PID_GPIOCOM4	0x6A
#define PCH_PCR_PID_GPIOCOM2	0x6C
#define PCH_PCR_PID_GPIOCOM1	0x6D
#define PCH_PCR_PID_GPIOCOM0	0x6E
#define PCH_PCR_PID_ITSS	0xC4

int icelake_get_gpe(int gpe);

#endif /* __DRIVERS_SOC_ICELAKE_H__ */

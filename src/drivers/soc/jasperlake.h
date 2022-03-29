/*
 * Copyright (C) 2019 Intel Corporation.
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

#ifndef __DRIVERS_SOC_JASPERLAKE_H__
#define __DRIVERS_SOC_JASPERLAKE_H__

#include <stdint.h>
#include "drivers/soc/common/intel_gpe.h"
#include "drivers/soc/common/iomap.h"
#include "drivers/soc/intel_common.h"

#define PCH_DEV_SDCARD	PCI_DEV(0, 0x14, 5)
#define PCH_DEV_PCIE0	PCI_DEV(0, 0x1c, 0)
#define PCH_DEV_EMMC	PCI_DEV(0, 0x1a, 0)

/* Icelake PCR access to GPIO registers. */
#define PCH_PCR_BASE_ADDRESS	0xfd000000

#define GPE0_STS_OFF            0x60

/* I2C Designware Controller runs at 120 MHz */
#define JASPERLAKE_DW_I2C_MHZ   120

/* PCR PIDs. */
#define PCH_PCR_PID_GPIOCOM5	0x69
#define PCH_PCR_PID_GPIOCOM4	0x6A
#define PCH_PCR_PID_GPIOCOM2	0x6C
#define PCH_PCR_PID_GPIOCOM1	0x6D
#define PCH_PCR_PID_GPIOCOM0	0x6E
#define PCH_PCR_PID_ITSS	0xC4

static inline int jasperlake_get_gpe(int gpe)
{
	const SocGpeConfig cfg = {
		.gpe_max = GPE_MAX,
		.acpi_base = ACPI_BASE_ADDRESS,
		.gpe0_sts_off = GPE0_STS_OFF,
	};

	return soc_get_gpe(gpe, &cfg);
}
#endif /* __DRIVERS_SOC_JASPERLAKE_H__ */

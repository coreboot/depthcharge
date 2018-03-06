/*
 * Copyright 2012 Google Inc.
 * Copyright (C) 2017 Advanced Micro Devices, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_POWER_PCH_H__
#define __DRIVERS_POWER_PCH_H__

#include "drivers/power/power.h"

#define ACPI_MMIO_REGION	0xfed80000
#define PMIO_REGS		(phys_to_virt(ACPI_MMIO_REGION) + 0x300)

#define AMD_FCH_VID		0x1022
#define LPC_DEV			PCI_DEV(0, 0x14, 3)

/* Power Management registers */
#define POWER_RESET_CONFIG	(PMIO_REGS + 0x10)
#define   TOGGLE_ALL_PWR_GOOD_ON_CF9	(1 << 1)

#define ACPI_PM1_EVT_BLK	0x60
#define ACPI_PM1_CNT_BLK	0x62
#define ACPI_GPE0_BLK		0x68

/* AcpiPmEvtBlk offsets & registers */
#define PM1_STS			0x00
#define   PWRBTN_STS		(1 << 8)
#define PM1_EN			0x02

/* AcpiGpe0Blk Offsets */
#define GPE_EVENT_STATUS	0x00
#define GPE_EVENT_ENABLE	0x04

/* Register values for PmControl */
#define   SLP_EN		(1 << 13)
#define   SLP_TYP		(7 << 10)
#define   SLP_TYP_S0		(0 << 10)
#define   SLP_TYP_S1		(1 << 10)
#define   SLP_TYP_S3		(3 << 10)
#define   SLP_TYP_S4		(4 << 10)
#define   SLP_TYP_S5		(5 << 10)

#define RST_CNT			0xcf9
#define   SYS_RST		(1 << 1)
#define   RST_CPU		(1 << 2)
#define   FULL_RST		(1 << 3)

PowerOps kern_power_ops;

#endif /* __DRIVERS_POWER_PCH_H__ */

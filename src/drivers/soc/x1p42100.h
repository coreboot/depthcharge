/*
 * This file is part of the coreboot project.
 *
 * Copyright (c) 2025 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_SOC_QCOM_X1P42100_H__
#define __DRIVERS_SOC_QCOM_X1P42100_H__

#define QCOM_XHCI_TYPE_A_BASE		0x0A400000
#define QCOM_XHCI_TYPE_C0_BASE		0x0A600000
#define QCOM_XHCI_TYPE_C1_BASE		0x0A800000

#define QSPI_BASE			0x088DC000
#define TLMM_TILE_BASE			0x0F100000

#define QSPI_CS				GPIO(132)

/*
 * QUP SERIAL ENGINE BASE ADDRESSES
 */
/* QUPV3_0 */
#define QUP_SERIAL0_BASE		0x00B80000
#define QUP_SERIAL1_BASE		0x00B84000
#define QUP_SERIAL2_BASE		0x00B88000
#define QUP_SERIAL3_BASE		0x00B8C000
#define QUP_SERIAL4_BASE		0x00B90000
#define QUP_SERIAL5_BASE		0x00B94000
#define QUP_SERIAL6_BASE		0x00B98000
#define QUP_SERIAL7_BASE		0x00B9C000

/* QUPV3_1 */
#define QUP_SERIAL8_BASE		0x00A80000
#define QUP_SERIAL9_BASE		0x00A84000
#define QUP_SERIAL10_BASE		0x00A88000
#define QUP_SERIAL11_BASE		0x00A8C000
#define QUP_SERIAL12_BASE		0x00A90000
#define QUP_SERIAL13_BASE		0x00A94000
#define QUP_SERIAL14_BASE		0x00A98000
#define QUP_SERIAL15_BASE		0x00A9C000

/* QUPV3_2 */
#define QUP_SERIAL16_BASE		0x00880000
#define QUP_SERIAL17_BASE		0x00884000
#define QUP_SERIAL18_BASE		0x00888000
#define QUP_SERIAL19_BASE		0x0088C000
#define QUP_SERIAL20_BASE		0x00890000
#define QUP_SERIAL21_BASE		0x00894000
#define QUP_SERIAL22_BASE		0x00898000
#define QUP_SERIAL23_BASE		0x0089C000

/* TCSR */
#define CORE_TOP_CSR_BASE		0x01f00000
#define TCSR_TCSR_REGS_REG_BASE		(CORE_TOP_CSR_BASE + 0x000c0000)
#define HWIO_TCSR_TZ_WONCE_0_ADDR	(TCSR_TCSR_REGS_REG_BASE + 0x00014000)
#define HWIO_TCSR_TZ_WONCE_1_ADDR	(TCSR_TCSR_REGS_REG_BASE + 0x00014004)

#endif /*  __DRIVERS_SOC_QCOM_X1P42100_H__ */

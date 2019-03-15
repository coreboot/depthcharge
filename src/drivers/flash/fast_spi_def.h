/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of the coreboot project.
 *
 * Copyright 2017 Intel Corporation.
 * Copyright 2018 Google LLC
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

#ifndef __DRIVERS_FLASH_FAST_SPI_DEF_H__
#define __DRIVERS_FLASH_FAST_SPI_DEF_H__

#define SPIBAR_HSFSTS_CTL		0x04
#define SPIBAR_FADDR			0x08
#define SPIBAR_FDATA(n)			(0x10 + ((n) & 0xf) * 4)
#define SPIBAR_FREG(n)			(0x54 + (4 * n))
#define SPIBAR_FPR_BASE			0x84
#define SPIBAR_PTINX			0xcc
#define SPIBAR_PTDATA			0xd0

/* Bit definitions for FREG registers */
#define SPIBAR_FREG_BASE_MASK		(0x7fff)
#define SPIBAR_FREG_LIMIT_SHIFT		(16)
#define SPIBAR_FREG_LIMIT_MASK		(0x7fff << SPIBAR_FREG_LIMIT_SHIFT)

/* Bit definitions for FPR registers */
#define SPIBAR_FPR_MAX			5
#define SPIBAR_FPR_SHIFT		12
#define SPIBAR_FPR_MASK			0x7fff
#define SPIBAR_FPR_BASE_SHIFT		0
#define SPIBAR_FPR_LIMIT_SHIFT		16
#define SPIBAR_FPR_RPE			(1 << 15) /* Read Protect */
#define SPIBAR_FPR_WPE			(1 << 31) /* Write Protect */
#define SPIBAR_FPR(base, limit)	\
	(((((limit) >> SPIBAR_FPR_SHIFT) & SPIBAR_FPR_MASK) \
	  << SPIBAR_FPR_LIMIT_SHIFT) |	\
	 ((((base) >> SPIBAR_FPR_SHIFT) & SPIBAR_FPR_MASK) \
	  << SPIBAR_FPR_BASE_SHIFT))

/* Bit definitions for HSFSTS_CTL (0x04) register */
#define SPIBAR_HSFSTS_FDBC_MASK	(0x3f << 24)
#define SPIBAR_HSFSTS_FDBC(n)		(((n) << 24) & SPIBAR_HSFSTS_FDBC_MASK)
#define SPIBAR_HSFSTS_WET		(1 << 21)
#define SPIBAR_HSFSTS_FCYCLE_MASK	(0xf << 17)
#define SPIBAR_HSFSTS_FCYCLE(cyc)	(((cyc) << 17) \
					& SPIBAR_HSFSTS_FCYCLE_MASK)

/* Supported flash cycle types */
#define SPIBAR_HSFSTS_CYCLE_READ	SPIBAR_HSFSTS_FCYCLE(0)
#define SPIBAR_HSFSTS_CYCLE_WRITE	SPIBAR_HSFSTS_FCYCLE(2)
#define SPIBAR_HSFSTS_CYCLE_4K_ERASE	SPIBAR_HSFSTS_FCYCLE(3)
#define SPIBAR_HSFSTS_CYCLE_64K_ERASE	SPIBAR_HSFSTS_FCYCLE(4)
#define SPIBAR_HSFSTS_CYCLE_RD_ID	SPIBAR_HSFSTS_FCYCLE(6)
#define SPIBAR_HSFSTS_CYCLE_WR_STATUS	SPIBAR_HSFSTS_FCYCLE(7)
#define SPIBAR_HSFSTS_CYCLE_RD_STATUS	SPIBAR_HSFSTS_FCYCLE(8)

#define SPIBAR_HSFSTS_FGO		(1 << 16)
#define SPIBAR_HSFSTS_FLOCKDN		(1 << 15)
#define SPIBAR_HSFSTS_FDV		(1 << 14)
#define SPIBAR_HSFSTS_FDOPSS		(1 << 13)
#define SPIBAR_HSFSTS_WRSDIS		(1 << 11)
#define SPIBAR_HSFSTS_SAF_CE		(1 << 8)
#define SPIBAR_HSFSTS_SAF_ACTIVE	(1 << 7)
#define SPIBAR_HSFSTS_SAF_LE		(1 << 6)
#define SPIBAR_HSFSTS_SCIP		(1 << 5)
#define SPIBAR_HSFSTS_SAF_DLE		(1 << 4)
#define SPIBAR_HSFSTS_SAF_ERROR		(1 << 3)
#define SPIBAR_HSFSTS_AEL		(1 << 2)
#define SPIBAR_HSFSTS_FCERR		(1 << 1)
#define SPIBAR_HSFSTS_FDONE		(1 << 0)
#define SPIBAR_HSFSTS_W1C_BITS		(0xff)

/* Bit definitions for FADDR (0x08) register */
#define SPIBAR_FADDR_MASK		0x7FFFFFF

/* Maximum bytes of data that can fit in FDATAn (0x10) registers */
#define SPIBAR_FDATA_FIFO_SIZE		0x40

/* Bit definitions for PTINX (0xCC) register */
#define SPIBAR_PTINX_COMP_0		(0 << 14)
#define SPIBAR_PTINX_COMP_1		(1 << 14)
#define SPIBAR_PTINX_HORD_SFDP		(0 << 12)
#define SPIBAR_PTINX_HORD_PARAM		(1 << 12)
#define SPIBAR_PTINX_HORD_JEDEC		(2 << 12)
#define SPIBAR_PTINX_IDX_MASK		0xffc

#define SPIBAR_XFER_TIMEOUT_US		(5 * USECS_PER_SEC)
#define SPIBAR_PAGE_SIZE		256

#define  SPIBAR_BIOS_BFPREG		(0x0)
#define  BFPREG_BASE_MASK		(0x7fff)
#define  BFPREG_LIMIT_SHIFT		(16)
#define  BFPREG_LIMIT_MASK		(0x7fff << BFPREG_LIMIT_SHIFT)

#endif /* __DRIVERS_FLASH_FAST_SPI_DEF_H__ */

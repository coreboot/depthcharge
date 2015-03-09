/*
 * Copyright (C) 2015 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __DRIVERS_STORAGE_MTD_NAND_BCM_NAND_H__
#define __DRIVERS_STORAGE_MTD_NAND_BCM_NAND_H__

#include "drivers/storage/mtd/mtd.h"

#define BCM_NAND_BASE		0x18046000
#define BCM_NAND_IDM_BASE	0xf8105000

MtdDevCtrlr *new_bcm_nand(void *nand_base, void *nand_idm_base, int tmode);

#endif

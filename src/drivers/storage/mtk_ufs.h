/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2022 MediaTek Inc.
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

#ifndef __DRIVERS_STORAGE_MTK_UFS_H__
#define __DRIVERS_STORAGE_MTK_UFS_H__

#include <libpayload.h>

#include "drivers/storage/ufs.h"

typedef struct {
	UfsCtlr ufs;
	void *pericfg_base;
} MtkUfsCtlr;

#define PERI_MP_UFS_CTRL	0x448

#define BMSK_UFS_ASE_EN		BIT(4)
#define BMSK_UFS_RST_EN		BIT(3)
#define BMSK_UFS_LDO_EN		BIT(2)
#define BMSK_UFS_CK_EN		BIT(1)
#define BMSK_UFS_LP_EN		BIT(0)

MtkUfsCtlr *new_mtk_ufs_ctlr(uintptr_t hci_ioaddr, uintptr_t pericfg_ioaddr);

#endif // __DRIVERS_STORAGE_MTK_UFS_H__

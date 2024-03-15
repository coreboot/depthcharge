// SPDX-License-Identifier: GPL-2.0
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

#include <libpayload.h>

#include "drivers/storage/blockdev.h"
#include "drivers/storage/ufs.h"
#include "drivers/storage/mtk_ufs.h"

static void mtk_ufs_reset_device(UfsCtlr *ufs)
{
	MtkUfsCtlr *mtk_ufs = container_of(ufs, MtkUfsCtlr, ufs);

	if (!mtk_ufs->pericfg_base)
		return;

	/* reset device */
	clrbits32(mtk_ufs->pericfg_base + PERI_MP_UFS_CTRL, BMSK_UFS_RST_EN);

	udelay(10);

	setbits32(mtk_ufs->pericfg_base + PERI_MP_UFS_CTRL, BMSK_UFS_RST_EN);

	/* wait awhile after device reset */
	mdelay(10);
}

static int mtk_ufs_hook_fn(UfsCtlr *ufs, UfsHookOp op, void *data)
{
	switch (op) {
	case UFS_OP_PRE_HCE:
		mtk_ufs_reset_device(ufs);
		break;
	case UFS_OP_PRE_LINK_STARTUP:
		return ufs_dme_set(ufs, PA_LOCAL_TX_LCC_ENABLE, 0);
	case UFS_OP_POST_LINK_STARTUP:
		break;
	default:
		break;
	};
	return 0;
}

MtkUfsCtlr *new_mtk_ufs_ctlr(uintptr_t hci_ioaddr, uintptr_t pericfg_ioaddr)
{
	MtkUfsCtlr *mtk_ufs = xzalloc(sizeof(MtkUfsCtlr));

	mtk_ufs->ufs.bctlr.ops.update = ufs_update;
	mtk_ufs->ufs.bctlr.need_update = 1;
	mtk_ufs->ufs.hci_base = (void *)hci_ioaddr;
	mtk_ufs->ufs.hook_fn  = mtk_ufs_hook_fn;
	mtk_ufs->pericfg_base = (void *)pericfg_ioaddr;

	return mtk_ufs;
}

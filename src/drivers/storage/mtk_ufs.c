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

static int ufs_dme_configure_adapt(UfsCtlr *ufs, uint32_t agreed_gear, uint32_t adapt_val)
{
	if (agreed_gear < UFS_GEAR4)
		adapt_val = PA_NO_ADAPT;

	return ufs_dme_set(ufs, PA_TXHSADAPTTYPE, adapt_val);
}

static bool mtk_ufs_pmc_via_fastauto(UfsCtlr *ufs)
{
	UfsTfrMode *tfr_mode = &ufs->tfr_mode;

	if (ufs->unipro_version < UFS_UNIPRO_VERSION_1_8)
		return false;

	if ((tfr_mode->rx.pwr_mode != UFS_FAST_MODE) &&
	    (tfr_mode->rx.pwr_mode < UFS_GEAR4))
		return false;

	if ((tfr_mode->tx.pwr_mode != UFS_FAST_MODE) &&
	    (tfr_mode->tx.pwr_mode < UFS_GEAR4))
		return false;

	if ((tfr_mode->tx.pwr_mode == UFS_SLOW_MODE) ||
	    (tfr_mode->rx.pwr_mode == UFS_SLOW_MODE))
		return false;

	return true;
}

static int mtk_ufs_pre_pwr_mode_change(UfsCtlr *ufs)
{
	UfsTfrMode *tfr_mode = &ufs->tfr_mode;
	uint32_t tx_pwr_mode = tfr_mode->tx.pwr_mode;
	uint32_t rx_pwr_mode = tfr_mode->rx.pwr_mode;
	uint32_t tx_gear = tfr_mode->tx.gear;
	uint32_t rx_gear = tfr_mode->rx.gear;
	uint32_t mib_val;
	int rc = 0;

	/* Change power mode to FASTAUTO for compatible devices. */
	if (mtk_ufs_pmc_via_fastauto(ufs)) {
		rc = ufs_dme_peer_get(ufs, PA_MINRXTRAILINGCLOCKS, &mib_val);
		if (rc)
			return rc;
		rc = ufs_dme_set(ufs, PA_TXTRAILINGCLOCKS, mib_val);
		if (rc)
			return rc;

		rc = ufs_dme_get(ufs, PA_MINRXTRAILINGCLOCKS, &mib_val);
		if (rc)
			return rc;
		rc = ufs_dme_peer_set(ufs, PA_TXTRAILINGCLOCKS, mib_val);
		if (rc)
			return rc;

		rc = ufs_dme_set(ufs, PA_TXHSADAPTTYPE, PA_NO_ADAPT);
		if (rc)
			return rc;

		tfr_mode->tx.pwr_mode = UFS_FASTAUTO_MODE;
		tfr_mode->rx.pwr_mode = UFS_FASTAUTO_MODE;
		tfr_mode->tx.gear = UFS_GEAR1;
		tfr_mode->rx.gear = UFS_GEAR1;

		rc = ufs_pwr_mode_change(ufs);
		if (rc)
			return rc;

		tfr_mode->tx.pwr_mode = tx_pwr_mode;
		tfr_mode->rx.pwr_mode = rx_pwr_mode;
		tfr_mode->tx.gear = tx_gear;
		tfr_mode->rx.gear = rx_gear;
	}

	// Set adapt configuration PA_INITIAL_ADAPT for HS G4/G5
	if (ufs->unipro_version >= UFS_UNIPRO_VERSION_1_8) {
		if (tfr_mode->rx.pwr_mode == UFS_FAST_MODE ||
		    tfr_mode->rx.pwr_mode == UFS_FASTAUTO_MODE)
			rc = ufs_dme_configure_adapt(ufs, tfr_mode->tx.gear, PA_INITIAL_ADAPT);
		else
			rc = ufs_dme_configure_adapt(ufs, tfr_mode->tx.gear, PA_NO_ADAPT);
	}

	return rc;
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
	case UFS_OP_PRE_GEAR_SWITCH:
		return mtk_ufs_pre_pwr_mode_change(ufs);
	case UFS_OP_POST_GEAR_SWITCH:
		break;
	default:
		break;
	};
	return 0;
}

MtkUfsCtlr *new_mtk_ufs_ctlr(uintptr_t hci_ioaddr, uintptr_t pericfg_ioaddr)
{
	MtkUfsCtlr *mtk_ufs = xzalloc(sizeof(MtkUfsCtlr));

	mtk_ufs->ufs.bctlr.type = BLOCK_CTRL_UFS;
	mtk_ufs->ufs.bctlr.ops.update = ufs_update;
	mtk_ufs->ufs.bctlr.need_update = 1;
	mtk_ufs->ufs.hci_base = (void *)hci_ioaddr;
	mtk_ufs->ufs.hook_fn  = mtk_ufs_hook_fn;
	mtk_ufs->pericfg_base = (void *)pericfg_ioaddr;

	return mtk_ufs;
}

// SPDX-License-Identifier: GPL-2.0-only

#include <libpayload.h>
#include "drivers/storage/qcom_ufs.h"

static const struct qcom_ufs_phy_offsets x1p42100_phy_offsets = {
	.serdes = 0x0000,
	.pcs    = 0x0400,
	.tx     = 0x1000,
	.rx     = 0x1200,
	.tx2    = 0x1800,
	.rx2    = 0x1A00,
};

static const uint32_t x1p42100_phy_regs[QPHY_REGS_MAX] = {
	[QPHY_SW_RESET]         = 0x008,
	[QPHY_START_CTRL]       = 0x000,
	[QPHY_PCS_READY_STATUS] = 0x1a8,
	[QPHY_POWER_DOWN_CTRL]  = 0x004,
	[QPHY_LINECFG]          = 0x17c,
};

static const struct qcom_ufs_phy_reg x1p42100_phy_serdes[] = {
	QPHY_INIT_CFG(0x0110, 0xD9),
	QPHY_INIT_CFG(0x0174, 0x16),
	QPHY_INIT_CFG(0x003c, 0x11),
	QPHY_INIT_CFG(0x009c, 0x00),
	QPHY_INIT_CFG(0x0120, 0x01),
	QPHY_INIT_CFG_G4(0x0140, 0x44),
	QPHY_INIT_CFG_G4(0x00f4, 0x0F),
	QPHY_INIT_CFG_G5(0x00f4, 0x1F),
	QPHY_INIT_CFG(0x0148, 0x00),
	QPHY_INIT_CFG(0x0088, 0x41),
	QPHY_INIT_CFG_G4(0x0070, 0x0A),
	QPHY_INIT_CFG_G5(0x0070, 0x06),
	QPHY_INIT_CFG(0x0074, 0x18),
	QPHY_INIT_CFG(0x0078, 0x14),
	QPHY_INIT_CFG(0x0080, 0x7F),
	QPHY_INIT_CFG(0x0084, 0x06),
	QPHY_INIT_CFG_G4(0x0028, 0x4C),
	QPHY_INIT_CFG_G4(0x0010, 0x0A),
	QPHY_INIT_CFG_G4(0x0014, 0x18),
	QPHY_INIT_CFG_G4(0x0018, 0x14),
	QPHY_INIT_CFG_G4(0x0020, 0x99),
	QPHY_INIT_CFG_G4(0x0024, 0x07),
	QPHY_INIT_CFG(0x00fc, 0x1B),
	QPHY_INIT_CFG(0x0100, 0x1C),
};

static const struct qcom_ufs_phy_reg x1p42100_phy_tx[] = {
	QPHY_INIT_CFG(0x07c, 0x05),
	QPHY_INIT_CFG_G4(0x108, 0xCC),
	QPHY_INIT_CFG(0x030, 0x07),
};

static const struct qcom_ufs_phy_reg x1p42100_phy_rx[] = {
	QPHY_INIT_CFG(0x1ac, 0x0F),
	QPHY_INIT_CFG(0x0d4, 0x0C),
	QPHY_INIT_CFG_G5(0x0dc, 0x0C),
	QPHY_INIT_CFG_G5(0x0f0, 0x04),
	QPHY_INIT_CFG_G5(0x1bc, 0x14),
	QPHY_INIT_CFG_G5(0x0f4, 0x07),
	QPHY_INIT_CFG_G5(0x1c4, 0x0E),
	QPHY_INIT_CFG_G5(0x054, 0x02),
	QPHY_INIT_CFG_G5(0x010, 0x1C),
	QPHY_INIT_CFG_G5(0x024, 0x06),
	QPHY_INIT_CFG(0x178, 0x08),
	QPHY_INIT_CFG(0x208, 0xCE),
	QPHY_INIT_CFG(0x20c, 0xCE),
	QPHY_INIT_CFG(0x210, 0x18),
	QPHY_INIT_CFG(0x214, 0x1A),
	QPHY_INIT_CFG(0x218, 0x0F),
	QPHY_INIT_CFG(0x220, 0x60),
	QPHY_INIT_CFG(0x238, 0x9E),
	QPHY_INIT_CFG(0x244, 0x60),
	QPHY_INIT_CFG(0x25c, 0x9E),
	QPHY_INIT_CFG(0x260, 0x0E),
	QPHY_INIT_CFG(0x264, 0x36),
	QPHY_INIT_CFG(0x270, 0x02),
	QPHY_INIT_CFG_G5(0x280, 0xB9),
	QPHY_INIT_CFG_G5(0x284, 0x4F),
	QPHY_INIT_CFG_G5(0x2f8, 0x30),
	QPHY_INIT_CFG(0x058, 0x94),
};

static const struct qcom_ufs_phy_reg x1p42100_phy_pcs[] = {
	QPHY_INIT_CFG(0x1f4, 0x43),
	QPHY_INIT_CFG_G4(0x02c, 0x0B),
	QPHY_INIT_CFG_G5(0x02c, 0x33),
	QPHY_INIT_CFG(0x030, 0x0F),
	QPHY_INIT_CFG(0x18c, 0x67),
	QPHY_INIT_CFG_G4(0x074, 0x04),
	QPHY_INIT_CFG_G4(0x0bc, 0x04),
	QPHY_INIT_CFG_G5(0x12c, 0x4D),
	QPHY_INIT_CFG_G5(0x220, 0x9E),
	QPHY_INIT_CFG(0x1fc, 0x02),
};

const struct qcom_ufs_phy_cfg qcom_ufs_phy_cfg = {
	.phy_base = QCOM_UFS_PHY_BASE,
	.lanes    = 2,
	.offsets  = &x1p42100_phy_offsets,
	.tbls = {
		.serdes     = x1p42100_phy_serdes,
		.serdes_num = ARRAY_SIZE(x1p42100_phy_serdes),
		.tx         = x1p42100_phy_tx,
		.tx_num     = ARRAY_SIZE(x1p42100_phy_tx),
		.rx         = x1p42100_phy_rx,
		.rx_num     = ARRAY_SIZE(x1p42100_phy_rx),
		.pcs        = x1p42100_phy_pcs,
		.pcs_num    = ARRAY_SIZE(x1p42100_phy_pcs),
	},
	.regs     = x1p42100_phy_regs,
};

// SPDX-License-Identifier: GPL-2.0-only

#include <libpayload.h>
#include "drivers/storage/qcom_ufs.h"

static const struct qcom_ufs_phy_offsets dougie_phy_offsets = {
	.serdes = 0x0000,
	.tx     = 0x0400,
	.rx     = 0x0600,
	.tx2    = 0x0800,
	.rx2    = 0x0a00,
	.pcs    = 0x0c00,
};

static const uint32_t dougie_phy_regs[QPHY_REGS_MAX] = {
	[QPHY_SW_RESET]         = 0x008,
	[QPHY_START_CTRL]       = 0x000,
	[QPHY_PCS_READY_STATUS] = 0x180,
	[QPHY_POWER_DOWN_CTRL]  = 0x004,
	[QPHY_LINECFG]          = 0x148,
};

static const struct qcom_ufs_phy_reg dougie_phy_serdes[] = {
	QPHY_INIT_CFG(0x0094, 0xD9),
	QPHY_INIT_CFG(0x0158, 0x11),
	QPHY_INIT_CFG(0x015c, 0x00),
	QPHY_INIT_CFG(0x00a4, 0x01),
	QPHY_INIT_CFG(0x010c, 0x06),
	QPHY_INIT_CFG(0x0058, 0x0F),
	QPHY_INIT_CFG(0x0124, 0x00),
	QPHY_INIT_CFG(0x01bc, 0x11),
	QPHY_INIT_CFG(0x00bc, 0x82),
	QPHY_INIT_CFG(0x0074, 0x06),
	QPHY_INIT_CFG(0x007c, 0x16),
	QPHY_INIT_CFG(0x0084, 0x36),
	QPHY_INIT_CFG(0x00ac, 0xFF),
	QPHY_INIT_CFG(0x00b0, 0x0C),
	QPHY_INIT_CFG(0x01ac, 0xAC),
	QPHY_INIT_CFG(0x01b0, 0x1E),
	QPHY_INIT_CFG(0x00c4, 0x98),
	QPHY_INIT_CFG(0x0078, 0x06),
	QPHY_INIT_CFG(0x0080, 0x16),
	QPHY_INIT_CFG(0x0088, 0x36),
	QPHY_INIT_CFG(0x00b4, 0x32),
	QPHY_INIT_CFG(0x00b8, 0x0F),
	QPHY_INIT_CFG(0x01b4, 0xDD),
	QPHY_INIT_CFG(0x01b8, 0x23),
};

static const struct qcom_ufs_phy_reg dougie_phy_tx[] = {
	QPHY_INIT_CFG(0x0d8, 0x06),
	QPHY_INIT_CFG(0x0dc, 0x03),
	QPHY_INIT_CFG(0x0e0, 0x01),
	QPHY_INIT_CFG(0x0e4, 0x00),
	QPHY_INIT_CFG_G4(0x084, 0xE5),
	QPHY_INIT_CFG(0x0b8, 0x0C),
};

static const struct qcom_ufs_phy_reg dougie_phy_rx[] = {
	QPHY_INIT_CFG(0x120, 0x24),
	QPHY_INIT_CFG(0x11c, 0x0F),
	QPHY_INIT_CFG(0x124, 0x1E),
	QPHY_INIT_CFG(0x128, 0x18),
	QPHY_INIT_CFG(0x030, 0x0A),
	QPHY_INIT_CFG(0x034, 0x5A),
	QPHY_INIT_CFG(0x044, 0xF1),
	QPHY_INIT_CFG(0x03c, 0x80),
	QPHY_INIT_CFG_G4(0x048, 0x81),
	QPHY_INIT_CFG(0x008, 0x0E),
	QPHY_INIT_CFG(0x014, 0x04),
	QPHY_INIT_CFG_G4(0x080, 0x6F),
	QPHY_INIT_CFG_G4(0x0e8, 0x04),
	QPHY_INIT_CFG_G4(0x0ec, 0x00),
	QPHY_INIT_CFG_G4(0x0f0, 0x09),
	QPHY_INIT_CFG_G4(0x0f4, 0x07),
	QPHY_INIT_CFG_G4(0x110, 0x17),
	QPHY_INIT_CFG(0x114, 0x00),
	QPHY_INIT_CFG_G4(0x100, 0x20),
	QPHY_INIT_CFG_G4(0x0f8, 0x80),
	QPHY_INIT_CFG_G4(0x0fc, 0x01),
	QPHY_INIT_CFG_G4(0x170, 0x3F),
	QPHY_INIT_CFG_G4(0x174, 0xFF),
	QPHY_INIT_CFG_G4(0x178, 0xFF),
	QPHY_INIT_CFG_G4(0x17c, 0x7F),
	QPHY_INIT_CFG_G4(0x180, 0x2C),
	QPHY_INIT_CFG_G4(0x184, 0x6D),
	QPHY_INIT_CFG_G4(0x188, 0x6D),
	QPHY_INIT_CFG_G4(0x18c, 0xED),
	QPHY_INIT_CFG(0x190, 0x3B),
	QPHY_INIT_CFG_G4(0x194, 0x3C),
	QPHY_INIT_CFG(0x198, 0xE0),
	QPHY_INIT_CFG(0x19c, 0xC8),
	QPHY_INIT_CFG(0x1a0, 0xC8),
	QPHY_INIT_CFG(0x1a4, 0x3B),
	QPHY_INIT_CFG(0x1a8, 0xB1),
	QPHY_INIT_CFG(0x1bc, 0x0C),
	QPHY_INIT_CFG_G4(0x0dc, 0x0F),
};

static const struct qcom_ufs_phy_reg dougie_phy_pcs[] = {
	QPHY_INIT_CFG(0x158, 0x6D),
	QPHY_INIT_CFG_G4(0x030, 0x10),
	QPHY_INIT_CFG(0x038, 0x02),
	QPHY_INIT_CFG(0x1d8, 0x43),
	QPHY_INIT_CFG(0x124, 0x1F),
	QPHY_INIT_CFG(0x150, 0xFF),
	QPHY_INIT_CFG_G4(0x02c, 0x0B),
	QPHY_INIT_CFG_G4(0x00c, 0x2D),
	QPHY_INIT_CFG_G4(0x010, 0xB0),
	QPHY_INIT_CFG_G4(0x160, 0xFF),
	QPHY_INIT_CFG_G4(0x168, 0x1B),
	QPHY_INIT_CFG_G4(0x074, 0x04),
	QPHY_INIT_CFG_G4(0x0b4, 0x04),
	QPHY_INIT_CFG_G4(0x060, 0x0A),
	QPHY_INIT_CFG(0x1e0, 0x02),
};

void qcom_ufs_dev_reset(void)
{
	write32((void *)TLMM_UFS_DEV_RESET, UFS_DEV_RESET_ASSERT);
	udelay(10);
	write32((void *)TLMM_UFS_DEV_RESET, UFS_DEV_RESET_DEASSERT);
	udelay(10);
}

const struct qcom_ufs_phy_cfg qcom_ufs_phy_cfg = {
	.phy_base = QCOM_UFS_PHY_BASE,
	.lanes    = 2,
	.offsets  = &dougie_phy_offsets,
	.tbls = {
		.serdes     = dougie_phy_serdes,
		.serdes_num = ARRAY_SIZE(dougie_phy_serdes),
		.tx         = dougie_phy_tx,
		.tx_num     = ARRAY_SIZE(dougie_phy_tx),
		.rx         = dougie_phy_rx,
		.rx_num     = ARRAY_SIZE(dougie_phy_rx),
		.pcs        = dougie_phy_pcs,
		.pcs_num    = ARRAY_SIZE(dougie_phy_pcs),
	},
	.regs     = dougie_phy_regs,
};

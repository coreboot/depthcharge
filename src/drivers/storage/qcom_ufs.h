/* SPDX-License-Identifier: GPL-2.0-only */


#ifndef __DRIVERS_STORAGE_QCOM_UFS_H__
#define __DRIVERS_STORAGE_QCOM_UFS_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "drivers/storage/ufs.h"
#include "drivers/soc/qcom_soc.h"

#define GDSCR_SW_COLLAPSE		BIT(0)
#define GDSCR_PWR_ON			BIT(31)

#define CBCR_CLK_ENABLE			BIT(0)
#define CBCR_FORCE_MEM_PERIPH_OFF	BIT(12)
#define CBCR_FORCE_MEM_PERIPH_ON	BIT(13)
#define CBCR_FORCE_MEM_CORE_ON		BIT(14)
#define CBCR_MEM_FLAGS_MASK		(CBCR_FORCE_MEM_PERIPH_OFF | \
					CBCR_FORCE_MEM_PERIPH_ON  | \
					CBCR_FORCE_MEM_CORE_ON)

#define RCG_CMD_UPDATE			BIT(0)

#define UFS_MEM_SYS1CLK_1US		0xc0
#define PA_VS_CORE_CLK_40NS_CYCLES	0x9007
#define DME_VS_CORE_CLK_CTRL		0xD002
#define DME_VS_CORE_CLK_CTRL_MAX_MASK	0x0FFF0000
#define DME_VS_CORE_CLK_CTRL_MAX_SHIFT	16
#define UFS_GDSC_TIMEOUT_US		2000
#define UFS_TX_FSM_TIMEOUT_US		100000
#define UFS_PCS_READY_TIMEOUT_MS	1000

#define UFS_PHY_RESET_DELAY_US		500
#define UFS_PHY_SOFT_RESET_DELAY_US	1000
#define UFS_GEAR_SWITCH_DELAY_US	500

#define PA_VS_CORE_CLK_40NS_MASK	0xF

#define PA_VS_CONFIG_REG1_VAL		0xd1
#define PA_LOCAL_TX_LCC_DISABLE		0
#define PA_PEER_TX_LCC_DISABLE		0

#define TX_FSM_STATE			0x41
#define TX_FSM_HIBERN8			1

#define UFS_MEM_CFG0			0xd8
#define UFS_MEM_CFG1			0xdc
#define CFG0_QUNIPRO_G4_SEL		BIT(5)
#define CFG1_UFS_DEV_REF_CLK_EN		BIT(26)
#define CFG1_UFS_PHY_SOFT_RESET		BIT(1)

#define RX_HS_ADAPT_INITIAL_CAPABILITY	0x9f

#define GEN_SELECT_RX(lane)		((lane) + 4)
#define GEN_SELECT_TX(lane)		(lane)

#define PA_PEERRXHSADAPTINITIAL		0x15d3

#define TX_HS_EQUALIZER_SETTING		0x37
#define TX_HS_DEEMPH_GEAR5		0x4
#define TX_HS_DEEMPH_DEFAULT		0x0
#define UFS_HS_G5_GEAR			5

struct qcom_ufs_phy_reg {
	uint32_t offset;
	uint8_t  val;
	uint8_t  gear_mask;
};

#define QPHY_G4 BIT(0)
#define QPHY_G5 BIT(1)
#define QPHY_BOTH (QPHY_G4 | QPHY_G5)

#define QPHY_INIT_CFG(o, v) \
	{ .offset = (o), .val = (v), .gear_mask = QPHY_BOTH }
#define QPHY_INIT_CFG_G4(o, v) \
	{ .offset = (o), .val = (v), .gear_mask = QPHY_G4 }
#define QPHY_INIT_CFG_G5(o, v) \
	{ .offset = (o), .val = (v), .gear_mask = QPHY_G5 }

struct qcom_ufs_phy_offsets {
	uint32_t serdes;
	uint32_t pcs;
	uint32_t tx;
	uint32_t rx;
	uint32_t tx2;
	uint32_t rx2;
};

struct qcom_ufs_phy_tbls {
	const struct qcom_ufs_phy_reg *serdes;
	size_t serdes_num;
	const struct qcom_ufs_phy_reg *tx;
	size_t tx_num;
	const struct qcom_ufs_phy_reg *rx;
	size_t rx_num;
	const struct qcom_ufs_phy_reg *pcs;
	size_t pcs_num;
};

enum qcom_ufs_phy_reg_idx {
	QPHY_SW_RESET,
	QPHY_START_CTRL,
	QPHY_PCS_READY_STATUS,
	QPHY_POWER_DOWN_CTRL,
	QPHY_LINECFG,
	QPHY_REGS_MAX,
};

#define UFS_PHY_LINECFG_VAL		0x02
#define MAX_SUPPORTED_LANES		2

#define PCS_READY			BIT(0)
#define SW_PWRDN			BIT(0)

struct qcom_ufs_phy_cfg {
	uintptr_t phy_base;
	int lanes;
	const struct qcom_ufs_phy_offsets *offsets;
	struct qcom_ufs_phy_tbls tbls;
	const uint32_t *regs;
};

struct qcom_ufs_ctlr {
	UfsCtlr ufs;
	uint32_t avail_lanes;
	uint8_t gear_mask;
	const struct qcom_ufs_phy_cfg *phy_cfg;
};

struct qcom_ufs_ctlr *new_qcom_ufs_ctlr(uintptr_t hci_base);

extern const struct qcom_ufs_phy_cfg qcom_ufs_phy_cfg;

#endif

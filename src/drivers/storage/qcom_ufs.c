// SPDX-License-Identifier: GPL-2.0-only

#include <libpayload.h>
#include "drivers/storage/blockdev.h"
#include "drivers/storage/ufs.h"
#include "drivers/storage/qcom_ufs.h"
#include "drivers/timer/timer.h"

static void *const gcc = (void *)QCOM_GCC_BASE;

static void gcc_write32(uint32_t offset, uint32_t value)
{
	write32(gcc + offset, value);
}

static void ufs_enable_gdsc(void)
{
	struct stopwatch sw;

	clrbits_le32(gcc + GCC_UFS_PHY_GDSCR, GDSCR_SW_COLLAPSE);
	stopwatch_init_usecs_expire(&sw, UFS_GDSC_TIMEOUT_US);
	while (!(read32(gcc + GCC_UFS_PHY_GDSCR) & GDSCR_PWR_ON)) {
		if (stopwatch_expired(&sw))
			break;
	}

	clrbits_le32(gcc + GCC_UFS_MEM_PHY_GDSCR, GDSCR_SW_COLLAPSE);
	stopwatch_init_usecs_expire(&sw, UFS_GDSC_TIMEOUT_US);
	while (!(read32(gcc + GCC_UFS_MEM_PHY_GDSCR) & GDSCR_PWR_ON)) {
		if (stopwatch_expired(&sw))
			break;
	}
}

static void ufs_enable_clocks(void)
{
	gcc_write32(GCC_UFS_PHY_AXI_CFG_RCGR, RCG_CFG_SRC_SEL_GPLL0 | RCG_CFG_SRC_DIV_2);
	gcc_write32(GCC_UFS_PHY_AXI_CMD_RCGR, RCG_CMD_UPDATE);

	gcc_write32(GCC_UFS_PHY_UNIPRO_CORE_CFG_RCGR, RCG_CFG_SRC_SEL_GPLL0 | RCG_CFG_SRC_DIV_2);
	gcc_write32(GCC_UFS_PHY_UNIPRO_CORE_CMD_RCGR, RCG_CMD_UPDATE);

	gcc_write32(GCC_UFS_PHY_ICE_CORE_CFG_RCGR, RCG_CFG_SRC_SEL_GPLL4 | RCG_CFG_SRC_DIV_2);
	gcc_write32(GCC_UFS_PHY_ICE_CORE_CMD_RCGR, RCG_CMD_UPDATE);

	clrsetbits_le32(gcc + GCC_UFS_PHY_AXI_CBCR,
			CBCR_MEM_FLAGS_MASK, CBCR_CLK_ENABLE);
	setbits_le32(gcc + GCC_UFS_PHY_AHB_CBCR, CBCR_CLK_ENABLE);
	clrsetbits_le32(gcc + GCC_UFS_PHY_UNIPRO_CORE_CBCR,
			CBCR_MEM_FLAGS_MASK, CBCR_CLK_ENABLE);
	clrsetbits_le32(gcc + GCC_UFS_PHY_ICE_CORE_CBCR,
			CBCR_FORCE_MEM_PERIPH_ON | CBCR_FORCE_MEM_PERIPH_OFF,
			CBCR_CLK_ENABLE | CBCR_FORCE_MEM_CORE_ON);
	setbits_le32(gcc + GCC_UFS_PHY_PHY_AUX_CBCR, CBCR_CLK_ENABLE);
	setbits_le32(gcc + GCC_AGGRE_UFS_PHY_AXI_CBCR, CBCR_CLK_ENABLE);
	setbits_le32(gcc + GCC_UFS_PHY_TX_SYMBOL_0_CBCR, CBCR_CLK_ENABLE);
	setbits_le32(gcc + GCC_UFS_PHY_RX_SYMBOL_0_CBCR, CBCR_CLK_ENABLE);
	setbits_le32(gcc + GCC_UFS_PHY_RX_SYMBOL_1_CBCR, CBCR_CLK_ENABLE);
}

static void qcom_ufs_phy_write_tbl(struct qcom_ufs_ctlr *qcom_ufs, uintptr_t base,
				    const struct qcom_ufs_phy_reg *tbl, size_t count)
{
	if (!tbl)
		return;

	for (size_t i = 0; i < count; i++) {
		if (tbl[i].gear_mask & qcom_ufs->gear_mask)
			write32((void *)(base + tbl[i].offset), tbl[i].val);
	}
}

static void qcom_ufs_phy_init_tbls(struct qcom_ufs_ctlr *qcom_ufs, int lanes)
{
	const struct qcom_ufs_phy_cfg *cfg = qcom_ufs->phy_cfg;
	const struct qcom_ufs_phy_tbls *tbls = &cfg->tbls;
	uintptr_t base = cfg->phy_base;
	const struct qcom_ufs_phy_offsets *offs = cfg->offsets;

	qcom_ufs_phy_write_tbl(qcom_ufs, base + offs->serdes, tbls->serdes, tbls->serdes_num);
	qcom_ufs_phy_write_tbl(qcom_ufs, base + offs->tx, tbls->tx, tbls->tx_num);
	qcom_ufs_phy_write_tbl(qcom_ufs, base + offs->rx, tbls->rx, tbls->rx_num);

	if (lanes >= MAX_SUPPORTED_LANES) {
		qcom_ufs_phy_write_tbl(qcom_ufs, base + offs->tx2, tbls->tx, tbls->tx_num);
		qcom_ufs_phy_write_tbl(qcom_ufs, base + offs->rx2, tbls->rx, tbls->rx_num);
	}

	qcom_ufs_phy_write_tbl(qcom_ufs, base + offs->pcs, tbls->pcs, tbls->pcs_num);
}

static int qcom_ufs_pre_hce(struct qcom_ufs_ctlr *qcom_ufs)
{
	UfsCtlr *ufs = &qcom_ufs->ufs;
	const struct qcom_ufs_phy_cfg *cfg = qcom_ufs->phy_cfg;
	void *pcs = (void *)(cfg->phy_base + cfg->offsets->pcs);
	struct stopwatch sw;
	int rc;
	uint32_t lanes;

	ufs_enable_gdsc();

	gcc_write32(GCC_UFS_PHY_BCR, 1);
	udelay(UFS_PHY_RESET_DELAY_US);
	gcc_write32(GCC_UFS_PHY_BCR, 0);

	ufs_enable_clocks();

	rc = ufs_dme_get(ufs, PA_AVAILTXDATALANES, &qcom_ufs->avail_lanes);
	if (rc) {
		printf("%s: DME_GET PA_AvailTxDataLanes failed (%d)\n",
		       __func__, rc);
		return rc;
	}

	lanes = qcom_ufs->avail_lanes;
	if (lanes == 0 || lanes > cfg->lanes) {
		printf("%s: Invalid PA_AvailTxDataLanes = %u (hw max = %d)\n",
		       __func__, lanes, cfg->lanes);
		return -1;
	}

	ufs_write32(ufs, UFS_MEM_CFG1,
		    ufs_read32(ufs, UFS_MEM_CFG1) | CFG1_UFS_PHY_SOFT_RESET);
	udelay(UFS_PHY_SOFT_RESET_DELAY_US);

	write32((void *)TCSR_UFS_PHY_CLKREF_EN, 1);

	write32(pcs + cfg->regs[QPHY_SW_RESET], 1);
	write32(pcs + cfg->regs[QPHY_POWER_DOWN_CTRL], SW_PWRDN);

	qcom_ufs_phy_init_tbls(qcom_ufs, lanes);

	write32(pcs + cfg->regs[QPHY_SW_RESET], 0);

	ufs_write32(ufs, UFS_MEM_CFG1,
		    ufs_read32(ufs, UFS_MEM_CFG1) & ~CFG1_UFS_PHY_SOFT_RESET);

	write32(pcs + cfg->regs[QPHY_START_CTRL], 1);

	stopwatch_init_msecs_expire(&sw, UFS_PCS_READY_TIMEOUT_MS);
	while (!(read32(pcs + cfg->regs[QPHY_PCS_READY_STATUS]) & PCS_READY)) {
		if (stopwatch_expired(&sw)) {
			printf("%s: timeout waiting PCS_READY, status=0x%08x\n",
			       __func__,
			       read32(pcs + cfg->regs[QPHY_PCS_READY_STATUS]));
			break;
		}
	}

	ufs_write32(ufs, UFS_MEM_CFG1,
		    ufs_read32(ufs, UFS_MEM_CFG1) | CFG1_UFS_DEV_REF_CLK_EN);

	if (qcom_ufs->gear_mask & QPHY_G5) {
		ufs_write32(ufs, UFS_MEM_CFG0,
			    ufs_read32(ufs, UFS_MEM_CFG0) & ~CFG0_QUNIPRO_G4_SEL);
	} else {
		ufs_write32(ufs, UFS_MEM_CFG0,
			    ufs_read32(ufs, UFS_MEM_CFG0) | CFG0_QUNIPRO_G4_SEL);
	}

	return 0;
}

static int dme_config_core_clk_ctrl(UfsCtlr *ufs)
{
	uint32_t val;
	int rc;

	rc = ufs_dme_get(ufs, DME_VS_CORE_CLK_CTRL, &val);
	if (rc)
		return rc;

	val = (val & ~DME_VS_CORE_CLK_CTRL_MAX_MASK) |
	      ((uint32_t)UFS_NOM_MAX_CORE_CLK_1US << DME_VS_CORE_CLK_CTRL_MAX_SHIFT);
	return ufs_dme_set(ufs, DME_VS_CORE_CLK_CTRL, val);
}

static int dme_config_core_clk_40ns(UfsCtlr *ufs)
{
	uint32_t val;
	int rc;

	rc = ufs_dme_get(ufs, PA_VS_CORE_CLK_40NS_CYCLES, &val);
	if (rc)
		return rc;

	val = (val & ~PA_VS_CORE_CLK_40NS_MASK) | UFS_NOM_PA_VS_40NS;
	return ufs_dme_set(ufs, PA_VS_CORE_CLK_40NS_CYCLES, val);
}

static int dme_set_vs_config_reg1(UfsCtlr *ufs)
{
	return ufs_dme_set(ufs, PA_VS_CONFIG_REG1, PA_VS_CONFIG_REG1_VAL);
}

static int dme_set_fc0_protection_timeout(UfsCtlr *ufs)
{
	return ufs_dme_set(ufs, DME_LOCALFC0PROTECTIONTIMEOUTVAL,
			   DL_FC0PROTECTIONTIMEOUTVAL);
}

static int dme_set_tc0_replay_timeout(UfsCtlr *ufs)
{
	return ufs_dme_set(ufs, DME_LOCALTC0REPLAYTIMEOUTVAL,
			   DL_TC0REPLAYTIMEOUTVAL);
}

static int dme_set_afc0_req_timeout(UfsCtlr *ufs)
{
	return ufs_dme_set(ufs, DME_LOCALAFC0REQTIMEOUTVAL,
			   DL_AFC0REQTIMEOUTVAL);
}

static int dme_disable_local_tx_lcc(UfsCtlr *ufs)
{
	return ufs_dme_set(ufs, PA_LOCAL_TX_LCC_ENABLE, PA_LOCAL_TX_LCC_DISABLE);
}

static int (*const dme_link_startup_funcs[])(UfsCtlr *) = {
	dme_config_core_clk_ctrl,
	dme_config_core_clk_40ns,
	dme_set_vs_config_reg1,
	dme_set_fc0_protection_timeout,
	dme_set_tc0_replay_timeout,
	dme_set_afc0_req_timeout,
	dme_disable_local_tx_lcc,
};

static int qcom_ufs_pre_link_startup(struct qcom_ufs_ctlr *qcom_ufs)
{
	UfsCtlr *ufs = &qcom_ufs->ufs;
	uint32_t fsm_state;
	struct stopwatch sw;
	int rc;

	ufs_write32(ufs, UFS_MEM_SYS1CLK_1US, UFS_NOM_SYSCLK_1US);

	stopwatch_init_usecs_expire(&sw, UFS_TX_FSM_TIMEOUT_US);
	do {
		rc = ufs_dme_get(ufs, TX_FSM_STATE, &fsm_state);
		if (rc == 0 && fsm_state == TX_FSM_HIBERN8)
			break;
	} while (!stopwatch_expired(&sw));

	if (fsm_state != TX_FSM_HIBERN8)
		printf("%s: WARNING: TX_FSM_State=%u (not HIBERN8)\n",
		       __func__, fsm_state);

	for (size_t i = 0; i < ARRAY_SIZE(dme_link_startup_funcs); i++) {
		rc = dme_link_startup_funcs[i](ufs);
		if (rc)
			return rc;
	}

	return 0;
}

static int qcom_ufs_post_link_startup(struct qcom_ufs_ctlr *qcom_ufs)
{
	UfsCtlr *ufs = &qcom_ufs->ufs;
	const struct qcom_ufs_phy_cfg *cfg = qcom_ufs->phy_cfg;
	void *pcs = (void *)(cfg->phy_base + cfg->offsets->pcs);
	int rc;

	write32(pcs + cfg->regs[QPHY_LINECFG], UFS_PHY_LINECFG_VAL);

	rc = ufs_dme_peer_set(ufs, PA_PEER_TX_LCC_ENABLE, PA_PEER_TX_LCC_DISABLE);
	if (rc) {
		printf("%s: DME_PEER_SET PA_PEER_TX_LCC_ENABLE failed (%d)\n",
		       __func__, rc);
		return rc;
	}

	return 0;
}

static int dme_config_peer_rx_hs_adapt(UfsCtlr *ufs)
{
	uint32_t hs_adapt_cap = 0;
	int rc;

	rc = ufs_utp_uic_getset(ufs, UICCMDR_DME_PEER_GET,
				MIBattrSel(RX_HS_ADAPT_INITIAL_CAPABILITY,
					   GEN_SELECT_RX(0)),
				0, &hs_adapt_cap);
	if (rc)
		return rc;

	return ufs_dme_set(ufs, PA_PEERRXHSADAPTINITIAL, hs_adapt_cap);
}

static int dme_set_tx_hs_adapt_type(UfsCtlr *ufs)
{
	return ufs_dme_set(ufs, PA_TXHSADAPTTYPE, PA_INITIAL_ADAPT);
}

static int (*const dme_pre_gear_switch_funcs[])(UfsCtlr *) = {
	dme_config_peer_rx_hs_adapt,
	dme_set_tx_hs_adapt_type,
};

static int qcom_ufs_pre_gear_switch(struct qcom_ufs_ctlr *qcom_ufs)
{
	UfsCtlr *ufs = &qcom_ufs->ufs;
	int rc;

	for (size_t i = 0; i < ARRAY_SIZE(dme_pre_gear_switch_funcs); i++) {
		rc = dme_pre_gear_switch_funcs[i](ufs);
		if (rc)
			return rc;
	}

	return 0;
}

static int qcom_ufs_post_gear_switch(struct qcom_ufs_ctlr *qcom_ufs)
{
	UfsCtlr *ufs = &qcom_ufs->ufs;
	uint32_t gear = ufs->tfr_mode.rx.gear;
	uint32_t lanes = ufs->tfr_mode.rx.lanes;
	uint32_t deemph;
	int rc;

	udelay(UFS_GEAR_SWITCH_DELAY_US);

	deemph = (gear >= UFS_HS_G5_GEAR) ? TX_HS_DEEMPH_GEAR5 : TX_HS_DEEMPH_DEFAULT;

	for (size_t i = 0; i < lanes; i++) {
		rc = ufs_utp_uic_getset(ufs, UICCMDR_DME_SET,
					MIBattrSel(TX_HS_EQUALIZER_SETTING,
						   GEN_SELECT_TX(i)),
					deemph, NULL);
		if (rc) {
			printf("%s: DME_SET TX_HS_Equalizer_Setting lane %zu failed (%d)\n",
			       __func__, i, rc);
			return rc;
		}
	}

	return 0;
}

static int qcom_ufs_hook_fn(UfsCtlr *ufs, UfsHookOp op, void *data)
{
	struct qcom_ufs_ctlr *qcom_ufs = container_of(ufs, struct qcom_ufs_ctlr, ufs);

	switch (op) {
	case UFS_OP_PRE_HCE:
		return qcom_ufs_pre_hce(qcom_ufs);
	case UFS_OP_PRE_LINK_STARTUP:
		return qcom_ufs_pre_link_startup(qcom_ufs);
	case UFS_OP_POST_LINK_STARTUP:
		return qcom_ufs_post_link_startup(qcom_ufs);
	case UFS_OP_PRE_GEAR_SWITCH:
		return qcom_ufs_pre_gear_switch(qcom_ufs);
	case UFS_OP_POST_GEAR_SWITCH:
		return qcom_ufs_post_gear_switch(qcom_ufs);
	default:
		return 0;
	}
}

struct qcom_ufs_ctlr *new_qcom_ufs_ctlr(uintptr_t hci_base)
{
	struct qcom_ufs_ctlr *qcom_ufs = xzalloc(sizeof(*qcom_ufs));

	qcom_ufs->ufs.bctlr.type        = BLOCK_CTRL_UFS;
	qcom_ufs->ufs.bctlr.ops.update  = ufs_update;
	qcom_ufs->ufs.bctlr.need_update = 1;
	qcom_ufs->ufs.hci_base          = (void *)hci_base;
	qcom_ufs->ufs.hook_fn           = qcom_ufs_hook_fn;
	qcom_ufs->avail_lanes           = 0;
	qcom_ufs->gear_mask             = QPHY_G4;
	qcom_ufs->phy_cfg               = &qcom_ufs_phy_cfg;

	return qcom_ufs;
}

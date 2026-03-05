/*
 * Copyright 2020-2026 Google LLC.
 * Depthcharge Driver for Qualcomm SoundWire Master (SWRM)
 */

#include <delay.h>
#include <libpayload.h>
#include <drivers/soc/x1p42100.h>
#include <drivers/bus/soundwire/soundwire.h>
#include <drivers/bus/soundwire/qcom/sdw-slave.h>

/* Helper: write a 32-bit value to a SWRM register */
static inline void swrm_write(uintptr_t base, uint32_t offset, uint32_t val)
{
	write32((volatile uint32_t *)(base + offset), val);
}

static volatile lpass_audio_cc_wsa_mclk * const LPASS_AUDIO_CC_WSA_MCLK = (volatile lpass_audio_cc_wsa_mclk *)LPASS_AUDIO_CC_WSA_MCLK_BASE;
static volatile lpass_va_clkrst_6d * const LPASS_VA_CLK_RST = (volatile lpass_va_clkrst_6d *)LPASS_VA_CLK_RST_BASE;
static volatile lpass_rx_clkrst_6ac * const LPASS_RX_CLK_RST = (volatile lpass_rx_clkrst_6ac *)LPASS_RX_CLK_RST_BASE;
static volatile cdc_rx_inp_mux * const LPASS_WSA_CDC_RX_INP_MUX = (volatile cdc_rx_inp_mux *)LPASS_WSA_CDC_RX_INP_MUX_BASE;
static volatile cdc_boost * const LPASS_WSA_CDC_BOOST = (volatile cdc_boost *)LPASS_WSA_CDC_BOOST_BASE;
static volatile cdc_ec_hq * const LPASS_WSA_CDC_EC_HQ = (volatile cdc_ec_hq *)LPASS_WSA_CDC_EC_HQ_BASE;
static volatile cdc_cb_decode * const LPASS_WSA_CDC_CB_DECODE = (volatile cdc_cb_decode *)LPASS_WSA_CDC_CB_DECODE_BASE;
static volatile lpass_wsa_intrctrl_cdcrx * const LPASS_WSA_CDC_INTR_CTRL = (volatile lpass_wsa_intrctrl_cdcrx *)LPASS_WSA_CDC_INTR_CTRL_BASE;
static volatile lpass_wsa_lpaifirq * const LPASS_WSA_LPAIF_IRQ = (volatile lpass_wsa_lpaifirq *)LPASS_WSA_LPAIF_IRQ_BASE;
static volatile lpass_wsa_clkrst_6b * const LPASS_WSA_CLK_RST = (volatile lpass_wsa_clkrst_6b *)LPASS_WSA_CLK_RST_BASE;
static volatile lpass_tx_clkrst_6ae * const LPASS_TX_CLK_RST = (volatile lpass_tx_clkrst_6ae *)LPASS_TX_CLK_RST_BASE;
static volatile lpass_tlmm_lite_gpio * const LPASS_TLMM_LITE_GPIO10 = (volatile lpass_tlmm_lite_gpio *)LPASS_TLMM_LITE_GPIO10_BASE;
static volatile lpass_tlmm_lite_gpio * const LPASS_TLMM_LITE_GPIO11 = (volatile lpass_tlmm_lite_gpio *)LPASS_TLMM_LITE_GPIO11_BASE;

#define SWRM_DPN(base, dpn) \
	((volatile lpass_wsa_swrm_dpn *) \
	((base) + SWRM_DPN_BASE_OFFSET + ((dpn) * SWRM_DPN_STRIDE)))

static inline void swrm_dpn_cfg(uintptr_t base, uint8_t dpn, uint32_t bank0, uint32_t bank1, uint32_t bank2, uint32_t bank3)
{
	volatile lpass_wsa_swrm_dpn *dp = SWRM_DPN(base, dpn);
	dp->DPn_BLOCK_CTRL_1 = 0x00000000;
	dp->DPn_PORT_CTRL_BANK_m = bank0;
	dp->DPn_PORT_CTRL_BANK_m = bank1;
	dp->DPn_PORT_CTRL_BANK_m = bank2;
	dp->DPn_PORT_CTRL_BANK_m = bank3;
	dp->DPn_PORT_CTRL_2_BANK_m = 0x00000000;
	dp->DPn_HCONTROL_BANK_m = 0x00000001;
}

static void swrm_send_cmd_table(volatile CPUn *cpun, const u32 *cmds, size_t num_cmds)
{
	for (size_t i = 0; i < num_cmds; i++) {
		cpun->CPUn_CMD_FIFO_WR_CMD = cmds[i];
		mdelay(5);
	}
}

static void swrm_poll(volatile CPUn *cpun, uint32_t bit)
{
	int timeout_us = SWRM_POLL_TIMEOUT_US;
	while (!(cpun->CPUn_INTERRUPT_STATUS & bit) && timeout_us > 0) {
		udelay(100);
		timeout_us -= 100;
	}
	if (timeout_us <= 0)
		printk(BIOS_WARNING, "SWRM: Timeout waiting\n");
}

static void swr_enumeration(volatile lpass_wsa_swrm *swrm, volatile CPUn *cpun)
{
	swrm->COMP_SW_RESET = SWRM_COMP_SW_RESET_ASSERT;
	mdelay(10);

	swrm->CMD_FIFO_CFG = SWRM_CMD_FIFO_RESET;
	swrm->CMD_FIFO_CFG = SWRM_CMD_FIFO_RESET_ENABLE;
	cpun->CPUn_INTERRUPT_CLEAR = CPUN_INTERRUPT_CLEAR_ALL;
	cpun->CPUn_INTERRUPT_EN = 0x0;

	LPASS_WSA_CLK_RST->rst_ctrl_swr = WSA_SWR_RESET_ASSERT;
	mdelay(20);

	swrm->MCP_BUS_CTRL = SWRM_MCP_BUS_ENABLE;
	swrm->COMP_CFG = SWRM_COMP_ENABLE;
	swrm->LINK_MANAGER_EE = SWRM_LINK_MANAGER_ENABLE;

	cpun->CPUn_CLK_CTRL = CPUN_CLK_ENABLE;

	cpun->CPUn_INTERRUPT_EN |= CPUN_IRQ_ENABLE_MASK;
	swrm->ENUMERATOR_CFG = SWRM_ENUMERATOR_ENABLE;
	swrm_poll(cpun, CPUN_IRQ_NEW_SLAVE);
	cpun->CPUn_INTERRUPT_CLEAR = CPUN_IRQ_NEW_SLAVE;
	cpun->CPUn_INTERRUPT_CLEAR = CPUN_IRQ_CLEAR_ENUM_PHASE;

	swrm->CMD_FIFO_CMD = SWRM_CMD_ENUMERATE_SLAVE;
	swrm_poll(cpun, CPUN_IRQ_SLAVE_STATUS);
	cpun->CPUn_INTERRUPT_CLEAR = CPUN_IRQ_ENUM_MASK;

	swrm->CMD_FIFO_CFG = SWRM_CMD_FIFO_RESET_ENABLE;
	LPASS_WSA_CLK_RST->rst_ctrl_mclk = WSA_MCLK_ENABLE;
}

static void configure_lpaif_irq(void)
{
	LPASS_WSA_LPAIF_IRQ->LPAIF_IRQ_ENa = LPAIF_IRQ_ENABLE_RX_DMA;
	LPASS_WSA_LPAIF_IRQ->LPAIF_IRQ2_ENa = LPAIF_IRQ_DISABLE_ALL;
	LPASS_WSA_LPAIF_IRQ->LPAIF_IRQ3_ENa = LPAIF_IRQ_DISABLE_ALL;
	LPASS_WSA_LPAIF_IRQ->LPAIF_IRQ_CLEARa = LPAIF_IRQ_CLEAR_ALL;
	LPASS_WSA_LPAIF_IRQ->LPAIF_IRQ2_CLEARa = LPAIF_IRQ_CLEAR_ALL;
	LPASS_WSA_LPAIF_IRQ->LPAIF_IRQ3_CLEARa = LPAIF_IRQ_CLEAR_ALL;
}

static void configure_rx_paths(void)
{
	LPASS_WSA_CDC_RX_INP_MUX->RX_INT0_CFG0 = RX_INT_CFG_ENABLE;
	LPASS_WSA_CDC_RX_INP_MUX->RX_INT0_CFG0 = RX_INT0_CFG0_DEFAULT;
	LPASS_WSA_CDC_RX_INP_MUX->RX_INT0_CFG1 = RX_INT_CFG_DISABLE;
	LPASS_WSA_CDC_RX_INP_MUX->RX_INT1_CFG0 = RX_INT1_CFG0_ENABLE;
	LPASS_WSA_CDC_RX_INP_MUX->RX_INT1_CFG0 = RX_INT1_CFG0_DEFAULT;
	LPASS_WSA_CDC_RX_INP_MUX->RX_INT1_CFG1 = RX_INT_CFG_DISABLE;
	LPASS_WSA_CDC_RX_INP_MUX->RX_INT0_CFG1 = RX_INT_CFG_DISABLE;
	LPASS_WSA_CDC_RX_INP_MUX->RX_INT1_CFG1 = RX_INT_CFG_DISABLE;
	LPASS_WSA_CDC_RX_INP_MUX->RX_MIX_CFG0 = RX_MIX_CFG_ENABLE;
	LPASS_WSA_CDC_RX_INP_MUX->RX_MIX_CFG0 = RX_MIX_CFG_DP_INPUT;

	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX0_RX_PATH_CFG0 = RX_PATH_ENABLE;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX0_RX_PATH_CFG1 = RX_PATH_CFG1_DEFAULT;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX0_RX_PATH_CFG3 = RX_PATH_CFG3_DEFAULT;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX1_RX_PATH_CFG0 = RX_PATH_ENABLE;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX1_RX_PATH_CFG1 = RX_PATH_CFG1_DEFAULT;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX1_RX_PATH_CFG3 = RX_PATH_CFG3_DEFAULT;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX0_RX_VOL_CTL = RX_VOL_LVL;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX1_RX_VOL_CTL = RX_VOL_LVL;
}

static void configure_boost_and_cb(void)
{
	LPASS_WSA_CDC_BOOST->CDC_BOOST0_BOOST_PATH_CTL = CDC_BOOST_PATH_ENABLE;
	LPASS_WSA_CDC_BOOST->CDC_BOOST0_BOOST_CFG1 = CDC_BOOST_CFG1_DEFAULT;
	LPASS_WSA_CDC_BOOST->CDC_BOOST0_BOOST_CFG2 = CDC_BOOST_CFG2_DEFAULT;
	LPASS_WSA_CDC_BOOST->CDC_BOOST1_BOOST_PATH_CTL = CDC_BOOST_PATH_ENABLE;
	LPASS_WSA_CDC_BOOST->CDC_BOOST1_BOOST_CFG1 = CDC_BOOST_CFG1_DEFAULT;
	LPASS_WSA_CDC_BOOST->CDC_BOOST1_BOOST_CFG2 = CDC_BOOST_CFG2_DEFAULT;

	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX0_RX_PATH_DSMDEM_CTL = RX_PATH_DSM_ENABLE;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX1_RX_PATH_DSMDEM_CTL = RX_PATH_DSM_ENABLE;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_INTR_CTRL_PIN1_MASK0 = INTR_CTRL_PIN_MASK_DEFAULT;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_INTR_CTRL_PIN2_MASK0 = INTR_CTRL_PIN_MASK_DEFAULT;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_INTR_CTRL_LEVEL0 = INTR_CTRL_LEVEL_DEFAULT;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_INTR_CTRL_BYPASS0 = INTR_CTRL_BYPASS_DISABLE;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX0_RX_PATH_CFG1 = RX_PATH_CFG1_TUNED;

	LPASS_WSA_CDC_RX_INP_MUX->SOFTCLIP_CFG0 = SOFTCLIP_ENABLE;
	LPASS_WSA_CDC_EC_HQ->LPASS_WSA_CDC_SOFTCLIP0_CRC = SOFTCLIP_ENABLE;

	LPASS_WSA_CDC_CB_DECODE->CFG1 = CB_DECODE_CFG1_DEFAULT;
	LPASS_WSA_CDC_CB_DECODE->CFG2 = CB_DECODE_CFG2_DEFAULT;
	LPASS_WSA_CDC_CB_DECODE->CFG3 = CB_DECODE_CFG3_DEFAULT;
	LPASS_WSA_CDC_CB_DECODE->CFG4 = CB_DECODE_CFG4_DEFAULT;
	LPASS_WSA_CDC_CB_DECODE->CFG5 = CB_DECODE_CFG5_DEFAULT;
	LPASS_WSA_CDC_CB_DECODE->CFG6 = CB_DECODE_CFG6_DEFAULT;
	LPASS_WSA_CDC_CB_DECODE->CFG7 = CB_DECODE_CFG7_DEFAULT;
	LPASS_WSA_CDC_CB_DECODE->CFG8 = CB_DECODE_CFG8_DEFAULT;
	LPASS_WSA_CDC_CB_DECODE->CTL2 = CB_DECODE_CTL_DISABLE;
	LPASS_WSA_CDC_CB_DECODE->CTL1 = CB_DECODE_CTL_ENABLE;

	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX0_RX_VOL_CTL = RX_VOL_LVL;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX1_RX_VOL_CTL = RX_VOL_LVL;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX0_RX_PATH_CTL = RX_PATH_ON;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX1_RX_PATH_CTL = RX_PATH_ON;
}

static void swrm_reset_fs_sequence(void)
{
	LPASS_VA_CLK_RST->rst_ctrl_swr = LPASS_SWR_RESET_DEASSERT;
	LPASS_VA_CLK_RST->rst_ctrl_fs  = LPASS_FS_DISABLE;
	LPASS_WSA_CLK_RST->rst_ctrl_fs = LPASS_FS_DISABLE;
	LPASS_RX_CLK_RST->LPASS_RX_RX_CLK_RST_CTRL_FS_CNT_CONTROL = LPASS_FS_DISABLE;
	LPASS_TX_CLK_RST->LPASS_TX_TX_CLK_RST_CTRL_FS_CNT_CONTROL = LPASS_FS_DISABLE;
	LPASS_VA_CLK_RST->rst_ctrl_swr = LPASS_SWR_RESET_ASSERT;
	LPASS_VA_CLK_RST->rst_ctrl_fs  = LPASS_FS_ENABLE;
}

static void swrm_configure_frame_ctrl(volatile lpass_wsa_swrm *swrm)
{
	swrm->MCP_FRAME_CTRL_BANK_m0 = MCP_FRAME_CTRL0_DEFAULT;
	swrm->MCP_FRAME_CTRL_BANK_m0 = MCP_FRAME_CTRL0_COMMIT;
	swrm->MCP_FRAME_CTRL_BANK_m1 = MCP_FRAME_CTRL1_DEFAULT;
	swrm->MCP_FRAME_CTRL_BANK_m1 = MCP_FRAME_CTRL1_COMMIT;
	swrm->MCP_FRAME_CTRL_BANK_m1 = MCP_FRAME_CTRL1_ENABLE;
}

static void qcom_init_swrm(void *sndwlinkaddr){

	uintptr_t swrm_base = (uintptr_t)sndwlinkaddr;
	volatile lpass_wsa_swrm *swrm = (volatile lpass_wsa_swrm *)swrm_base;
	volatile CPUn *cpun = (volatile CPUn *)(swrm_base + SWRM_CPUN_OFFSET);

	LPASS_AUDIO_CC_WSA_MCLK->LPASS_AUDIO_CC_WSA_MCLK_MODE_MUXSEL = 0x00000000;
	mdelay(1);
	LPASS_VA_CLK_RST->rst_ctrl_mclk = LPASS_MCLK_ENABLE;
	swrm_reset_fs_sequence();

	configure_lpaif_irq();
	configure_rx_paths();
	configure_boost_and_cb();

	swrm->CMD_FIFO_CFG = SWRM_CMD_FIFO_RESET_ENABLE;
	LPASS_WSA_CLK_RST->rst_ctrl_mclk = WSA_MCLK_ENABLE;
	LPASS_TLMM_LITE_GPIO10->GPIO_CFG = TLMM_GPIO10_CFG_DEFAULT;
	LPASS_TLMM_LITE_GPIO11->GPIO_CFG = TLMM_GPIO11_CFG_DEFAULT;
	LPASS_WSA_CLK_RST->rst_ctrl_mclk = WSA_MCLK_ENABLE;

	LPASS_WSA_CLK_RST->rst_ctrl_swr = WSA_SWR_RESET_ASSERT;
	swrm_reset_fs_sequence();

	LPASS_WSA_CLK_RST->rst_ctrl_mclk = WSA_MCLK_ENABLE;
	LPASS_WSA_CLK_RST->rst_ctrl_swr  = WSA_SWR_RESET_ASSERT;

	swr_enumeration(swrm, cpun);
	swrm_configure_frame_ctrl(swrm);

	cpun->CPUn_INTERRUPT_CLEAR = CPUN_INTERRUPT_SPECIAL_DONE;
	cpun->CPUn_INTERRUPT_CLEAR = CPUN_INTERRUPT_STATUS_CHANGE;
	LPASS_WSA_CLK_RST->rst_ctrl_mclk = WSA_MCLK_ENABLE;

	swrm_send_cmd_table(cpun, wsa_dp1_cmds, ARRAY_SIZE(wsa_dp1_cmds));
	swrm_dpn_cfg(swrm_base, 1, 0x01000000, 0x01000007, 0x01000107, 0x01000107);
	cpun->CPUn_CMD_FIFO_WR_CMD = SWRM_CMD(0x0F, 0xF, 0x2, 0x0070);
	mdelay(20);
	swrm_send_cmd_table(cpun, wsa_tuning_cmds, ARRAY_SIZE(wsa_tuning_cmds));
	cpun->CPUn_CMD_FIFO_WR_CMD = SWRM_CMD(0x0F, 0xF, 0xD, 0x0070);
	mdelay(20);
}

/*
 * qcom_swrm_sendwack - Function sends one message through the Sndw interface and waits
 * for the corresponding ACK message.
 *
 * sndwlinkaddr - Soundwire Link controller address.
 * txcmd - Message to send.
 * deviceindex - Index of the Soudnwire endpoint device.
 * val - Pointer to store read-back value for read commands.
 */
static int qcom_swrm_sendwack(void *sndwlinkaddr, sndw_cmd txcmd,
				  uint32_t deviceindex, uint8_t *val)
{
	uintptr_t base = (uintptr_t)sndwlinkaddr;

	printk(BIOS_INFO, "SWRM Tx: [ID:%d][Addr:0x%x][Op:%s][Data:0x%x]\n",
		deviceindex, txcmd.regaddr, (txcmd.cmdtype == cmd_read) ? "RD" : "WR", txcmd.regdata);

	if (txcmd.cmdtype == cmd_write) {
		/* [31:24]=data [23:20]=dev [19:16]=cmd_id [15:0]=addr */
		swrm_write(base, SWRM_CMD_FIFO_WR_CMD, SWRM_CMD(txcmd.regdata, deviceindex, 0xF, txcmd.regaddr));
		udelay(SWRM_CMD_DELAY_US);
	} else {
		printk(BIOS_ERR, "Command not implemented\n");
		return -1;
	}

	return 0;
}

/*
 * read_endpointid - Reads the MIPI-standard 6-byte identification from a codec.
 * sndwlinkaddr - Soundwire Link controller address.
 * deviceindex - Index of the Soudnwire endpoint device.
 * codecinfo - Pointer to the codec information.
 */
static int read_endpointid(void *sndwlinkaddr, uint32_t deviceindex,
			   sndw_codec_info *codecinfo)
{
	uintptr_t base = (uintptr_t)sndwlinkaddr + SWRM_DEV_ID_1_OFFSET + (SWRM_DEV_ID_STRIDE * deviceindex);
	uint32_t dev_id_1 = read32p(base);
	uint32_t dev_id_2 = read32p(base + sizeof(dev_id_1));
	codecinfo->codecid.id[0] = (dev_id_1 >> 0) & 0xFF;
	codecinfo->codecid.id[1] = (dev_id_1 >> 8) & 0xFF;
	codecinfo->codecid.id[2] = (dev_id_1 >> 16) & 0xFF;
	codecinfo->codecid.id[3] = (dev_id_1 >> 24) & 0xFF;
	codecinfo->codecid.id[4] = (dev_id_2 >> 0) & 0xFF;
	codecinfo->codecid.id[5] = (dev_id_2 >> 8) & 0xFF;

	codecinfo->deviceindex = deviceindex;
	codecinfo->sndwlinkaddr = sndwlinkaddr;

	printk(BIOS_INFO, "SWRM: Endpoint ID for device %d: ", deviceindex);
	for (int i = 0; i < SNDW_DEV_ID_NUM; i++)
		printk(BIOS_INFO, "%02x ", codecinfo->codecid.id[i]);
	printk(BIOS_INFO, "\n");

	return 0;
}

/* qcom_swrm_enable - Initialize SWRM controller and enumerate codec */
static int qcom_swrm_enable(SndwOps *me, sndw_codec_info *codecinfo)
{
	Soundwire *bus = container_of(me, Soundwire, ops);
	uintptr_t base = (uintptr_t)bus->sndwlinkaddr;

	printk(BIOS_INFO, "SWRM%d: Initializing Master at 0x%p\n", bus->sndwlinkindex, (void *)base);
	qcom_init_swrm(bus->sndwlinkaddr);

	if (codecinfo)
		read_endpointid(bus->sndwlinkaddr, codecinfo->deviceindex, codecinfo);

	return 0;
}

static void qcom_swrm_reset(uintptr_t base)
{
	swrm_write(base, SWRM_COMP_SW_RESET, SWRM_COMP_SW_RESET_VALUE);
	mdelay(20);

	swrm_write(base, SWRM_CMD_FIFO_CFG, SWRM_CMD_FIFO_CFG_RESET);
	swrm_write(base, SWRM_CMD_FIFO_CFG, SWRM_CMD_FIFO_CFG_ENABLE);

	LPASS_WSA_CLK_RST->rst_ctrl_mclk = WSA_MCLK_DISABLE;
}

/*
 * qcom_swrm_disable - Quiesce the SWRM controller via software reset.
 * me - Pointer to the Soundwire ops.
 */
static int qcom_swrm_disable(SndwOps *me)
{
	Soundwire *bus = container_of(me, Soundwire, ops);
	uintptr_t base = (uintptr_t)bus->sndwlinkaddr;

	printk(BIOS_INFO, "Disabling SWRM (0x%p)\n", (void *)base);
	qcom_swrm_reset(base);
	return 0;
}

/*
 * new_soundwire - Allocate and initialize a Qualcomm SoundWire bus context.
 * sndwlinkindex: Sndw Link Number (ignored)
 * sndwlinkaddr: Sndw Link Address
 */
Soundwire *new_soundwire(int sndwlinkindex, void *sndwlinkaddr)
{
	if (!sndwlinkaddr)
		return NULL;

	Soundwire *bus = xzalloc(sizeof(*bus));

	bus->ops.sndw_enable   = &qcom_swrm_enable;
	bus->ops.sndw_sendwack = &qcom_swrm_sendwack;
	bus->ops.sndw_disable  = &qcom_swrm_disable;
	bus->sndwlinkindex = sndwlinkindex;
	bus->sndwlinkaddr  = sndwlinkaddr;

	return bus;
}
/*
 * Copyright 2026 Google LLC.
 * Depthcharge Driver for Qualcomm SoundWire Master (SWRM)
 */

#ifndef __DRIVERS_BUS_SOUNDWIRE_QCOM_SWRM_H__
#define __DRIVERS_BUS_SOUNDWIRE_QCOM_SWRM_H__

#include <stdint.h>

typedef struct {
	uint32_t rst_ctrl_mclk;
	uint32_t rst_ctrl_fs;
	uint32_t rst_ctrl_swr;
} lpass_wsa_clkrst_6b;

typedef struct {
	uint32_t rst_ctrl_mclk;
	uint32_t rst_ctrl_fs;
	uint8_t _pad0[0x78];
	uint32_t rst_ctrl_swr;
} lpass_va_clkrst_6d;

typedef struct {
	uint8_t _pad0[0x104];
	uint32_t LPASS_RX_RX_CLK_RST_CTRL_FS_CNT_CONTROL;
} lpass_rx_clkrst_6ac;

typedef struct {
	uint32_t LPAIF_IRQ_ENa;
	uint8_t _pad0[0x8];
	uint32_t LPAIF_IRQ_CLEARa;
	uint8_t _pad1[0x4];
	uint32_t LPAIF_IRQ2_ENa;
	uint8_t _pad2[0x8];
	uint32_t LPAIF_IRQ2_CLEARa;
	uint8_t _pad3[0x4];
	uint32_t LPAIF_IRQ3_ENa;
	uint8_t _pad4[0x8];
	uint32_t LPAIF_IRQ3_CLEARa;
} lpass_wsa_lpaifirq;

typedef struct {
	uint32_t RX_INT0_CFG0;
	uint32_t RX_INT0_CFG1;
	uint32_t RX_INT1_CFG0;
	uint32_t RX_INT1_CFG1;
	uint32_t RX_MIX_CFG0;
	uint32_t RX_EC_CFG0;
	uint32_t SOFTCLIP_CFG0;
} cdc_rx_inp_mux;

typedef struct {
	uint32_t CTL1;
	uint32_t CTL2;
	uint8_t _pad1[0x4];
	uint32_t CFG1;
	uint32_t CFG2;
	uint32_t CFG3;
	uint32_t CFG4;
	uint32_t CFG5;
	uint32_t CFG6;
	uint32_t CFG7;
	uint32_t CFG8;
} cdc_cb_decode;

typedef struct {
	uint32_t CDC_BOOST0_BOOST_PATH_CTL;
	uint8_t _pad1[0x4];
	uint32_t CDC_BOOST0_BOOST_CFG1;
	uint32_t CDC_BOOST0_BOOST_CFG2;
	uint8_t _pad2[0x30];
	uint32_t CDC_BOOST1_BOOST_PATH_CTL;
	uint8_t _pad3[0x4];
	uint32_t CDC_BOOST1_BOOST_CFG1;
	uint32_t CDC_BOOST1_BOOST_CFG2;
} cdc_boost;

typedef struct {
	uint32_t LPASS_WSA_INTR_CTRL_PIN1_MASK0;
	uint8_t _pad1[0x1C];
	uint32_t LPASS_WSA_INTR_CTRL_PIN2_MASK0;
	uint8_t _pad2[0x3C];
	uint32_t LPASS_WSA_INTR_CTRL_LEVEL0;
	uint8_t _pad3[0x4];
	uint32_t LPASS_WSA_INTR_CTRL_BYPASS0;
	uint8_t _pad4[0x34];
	uint32_t LPASS_WSA_CDC_RX0_RX_PATH_CTL;
	uint32_t LPASS_WSA_CDC_RX0_RX_PATH_CFG0;
	uint32_t LPASS_WSA_CDC_RX0_RX_PATH_CFG1;
	uint32_t LPASS_WSA_CDC_RX0_RX_PATH_CFG2;
	uint32_t LPASS_WSA_CDC_RX0_RX_PATH_CFG3;
	uint32_t LPASS_WSA_CDC_RX0_RX_VOL_CTL;
	uint32_t LPASS_WSA_CDC_RX0_RX_PATH_MIX_CTL;
	uint8_t _pad5[0x30];
	uint32_t LPASS_WSA_CDC_RX0_RX_PATH_DSMDEM_CTL;
	uint8_t _pad6[0x30];
	uint32_t LPASS_WSA_CDC_RX1_RX_PATH_CTL;
	uint32_t LPASS_WSA_CDC_RX1_RX_PATH_CFG0;
	uint32_t LPASS_WSA_CDC_RX1_RX_PATH_CFG1;
	uint32_t LPASS_WSA_CDC_RX1_RX_PATH_CFG2;
	uint32_t LPASS_WSA_CDC_RX1_RX_PATH_CFG3;
	uint32_t LPASS_WSA_CDC_RX1_RX_VOL_CTL;
	uint32_t LPASS_WSA_CDC_RX1_RX_PATH_MIX_CTL;
	uint8_t _pad7[0x30];
	uint32_t LPASS_WSA_CDC_RX1_RX_PATH_DSMDEM_CTL;
} lpass_wsa_intrctrl_cdcrx;

typedef struct {
	uint32_t LPASS_WSA_CDC_SOFTCLIP0_CRC;
	uint8_t _pad1[0x3C];
	uint32_t CDC_EC_HQ0_EC_REF_HQ_PATH_CTL;
	uint32_t CDC_EC_HQ0_EC_REF_HQ_CFG0;
	uint8_t _pad2[0x38];
	uint32_t CDC_EC_HQ1_EC_REF_HQ_PATH_CTL;
	uint32_t CDC_EC_HQ1_EC_REF_HQ_CFG0;
} cdc_ec_hq;

typedef struct {
	uint32_t CPUn_INTERRUPT_STATUS;
	uint32_t CPUn_INTERRUPT_EN;
	uint32_t CPUn_INTERRUPT_CLEAR;
	uint8_t _pad1[0x14];
	uint32_t CPUn_CMD_FIFO_WR_CMD;
	uint32_t CPUn_CMD_FIFO_RD_CMD;
	uint8_t _pad2[0x38];
	uint32_t CPUn_CLK_CTRL;
} CPUn;

typedef struct {
	uint8_t _pad0[0xCC];
	uint32_t LPASS_AUDIO_CC_WSA_MCLK_2X_CBCR;
	uint8_t _pad1[0x4];
	uint32_t LPASS_AUDIO_CC_WSA_MCLK_CBCR;
	uint8_t _pad2[0x18];
	uint32_t LPASS_AUDIO_CC_TX_MCLK_2X_WSA_CBCR;
	uint32_t LPASS_AUDIO_CC_TX_MCLK_WSA_CBCR;
	uint8_t _pad3[0x8];
	uint32_t LPASS_AUDIO_CC_WSA_MCLK_MODE_MUXSEL;
} lpass_audio_cc_wsa_mclk;

typedef struct {
	uint32_t LPASS_AUDIO_CC_CODEC_MEM_CBCR;
	uint32_t LPASS_AUDIO_CC_CODEC_MEM0_CBCR;
	uint8_t _pad0[0x8];
	uint32_t LPASS_AUDIO_CC_CODEC_MEM1_CBCR;
	uint8_t _pad1[0x8];
	uint32_t LPASS_AUDIO_CC_CODEC_MEM2_CBCR;
	uint8_t _pad2[0x8];
	uint32_t LPASS_AUDIO_CC_CODEC_MEM3_CBCR;
} lpass_audiocc_codecmem;

typedef struct {
	uint8_t _pad0[0x4];
	uint32_t COMP_CFG;
	uint32_t COMP_SW_RESET;
	uint8_t _pad1[0xC];
	uint32_t LINK_MANAGER_EE;
	uint8_t _pad2[0x2EC];
	uint32_t CMD_FIFO_CMD;
	uint8_t _pad3[0x8];
	uint32_t CMD_FIFO_CFG;
	uint8_t _pad4[0x1E8];
	uint32_t ENUMERATOR_CFG;
	uint8_t _pad5[0xB18];
	uint32_t MCP_FRAME_CTRL_BANK_m0;
	uint8_t _pad6[0x24];
	uint32_t MCP_BUS_CTRL;
	uint8_t _pad7[0x14];
	uint32_t MCP_FRAME_CTRL_BANK_m1;
} lpass_wsa_swrm;

typedef struct {
	uint32_t GPIO_CFG;
	uint32_t GPIO_IN_OUT;
} lpass_tlmm_lite_gpio;

typedef struct {
	uint8_t _pad0[0x2C];
	uint32_t DPn_BLOCK_CTRL_1;
	uint8_t _pad1[0x34];
	uint32_t DPn_PORT_CTRL_BANK_m;
	uint32_t DPn_PORT_CTRL_2_BANK_m;
	uint8_t _pad2[0xC];
	uint32_t DPn_HCONTROL_BANK_m;
} lpass_wsa_swrm_dpn;

typedef struct {
	uint8_t _pad0[0x4];
	uint32_t LPASS_TX_TX_CLK_RST_CTRL_FS_CNT_CONTROL;
} lpass_tx_clkrst_6ae;

typedef struct {
	uint32_t rddma_ctl;
	uint32_t rddma_base;
	uint32_t rddma_buf_len;
	uint8_t _reserved1[0x4];
	uint32_t rddma_buf_per_len;
	uint8_t _reserved2[0x3C];
	uint32_t codec_intf;
} lpass_wsa_lpaifdmardregb_0;

#define SWRM_COMP_SW_RESET	0x0008
#define SWRM_CMD_FIFO_CFG	0x0314
#define SWRM_CMD_FIFO_WR_CMD	0x5020

#define SWRM_CPUN_OFFSET	0x5000UL
#define SWRM_DEV_ID_1_OFFSET	0x0530UL
#define SWRM_DEV_ID_2_OFFSET	0x0534UL
#define SWRM_DEV_ID_STRIDE	0x8UL

#define RX_PATH_ENABLE		0x00000002
#define RX_PATH_CFG1_DEFAULT	0x0000006C
#define RX_PATH_CFG3_DEFAULT	0x00000003
#define RX_VOL_LVL	0xFFFFFFE8
#define RX_PATH_ON	0x00000014

#define SWRM_DPN_BASE_OFFSET	0x1000UL
#define SWRM_DPN_STRIDE	0x100UL

#define SWRM_POLL_TIMEOUT_US	500000

#define SWRM_CMD(data, dev, id, addr) \
	( (((uint32_t)(data) & 0xFF) << 24) | \
	  (((uint32_t)(dev) & 0xF) << 20) | \
	  (((uint32_t)(id) & 0xF) << 16) | \
	  (((uint32_t)(addr) & 0xFFFF)) )

/* CPUn interrupt bits */
#define CPUN_IRQ_NEW_SLAVE	BIT(1)
#define CPUN_IRQ_SLAVE_STATUS	BIT(2)
#define CPUN_IRQ_SPECIAL_CMD_DONE		BIT(10)
#define CPUN_IRQ_CMD_ERROR	BIT(13)

#define CPUN_IRQ_ENUM_MASK \
	(CPUN_IRQ_NEW_SLAVE | CPUN_IRQ_SLAVE_STATUS)

#define CPUN_IRQ_ENABLE_MASK \
	(CPUN_IRQ_NEW_SLAVE | \
	 CPUN_IRQ_SLAVE_STATUS | \
	 CPUN_IRQ_SPECIAL_CMD_DONE | \
	 CPUN_IRQ_CMD_ERROR)

/* SWRM enumerator control */
#define SWRM_ENUMERATOR_ENABLE	BIT(0)
#define SWRM_CMD_ENUMERATE_SLAVE	BIT(0)

/* SWRM component control */
#define SWRM_COMP_SW_RESET_ASSERT	BIT(0)

/* SWRM CMD FIFO */
#define SWRM_CMD_FIFO_RESET	BIT(31)
#define SWRM_CMD_FIFO_ENABLE	(BIT(1) | BIT(0))
#define SWRM_CMD_FIFO_RESET_ENABLE	(SWRM_CMD_FIFO_RESET | SWRM_CMD_FIFO_ENABLE)

/* CPUn control */
#define CPUN_CLK_ENABLE	BIT(0)
#define CPUN_INTERRUPT_CLEAR_ALL		0xFFFFFFFF

/* WSA clock/reset */
#define WSA_SWR_RESET_ASSERT	BIT(0)
#define WSA_MCLK_ENABLE	BIT(0)

#define CPUN_IRQ_CLEAR_ENUM_PHASE	0x00002006

/* Delay after each SWRM register write */
#define SWRM_CMD_DELAY_US	100

/* VA / WSA clock reset control values */
#define LPASS_MCLK_ENABLE	BIT(0)

/* FS (frame sync) control */
#define LPASS_FS_DISABLE	BIT(1)
#define LPASS_FS_ENABLE		BIT(0)

/* SoundWire reset control states */
#define LPASS_SWR_RESET_DEASSERT	0x00000000
#define LPASS_SWR_RESET_ASSERT	BIT(1)

/* LPAIF IRQ control */
#define LPAIF_IRQ_ENABLE_RX_DMA	0x00008000
#define LPAIF_IRQ_DISABLE_ALL	0x00000000
#define LPAIF_IRQ_CLEAR_ALL	0xFFFFFFFF

/* RX input mux configuration */
#define RX_INT_CFG_ENABLE	0x00000001
#define RX_INT0_CFG0_DEFAULT	0x00000051
#define RX_INT1_CFG0_ENABLE	0x00000003
#define RX_INT1_CFG0_DEFAULT	0x00000063
#define RX_INT_CFG_DISABLE	0x00000000

/* RX mixer configuration */
#define RX_MIX_CFG_ENABLE	0x00000001
#define RX_MIX_CFG_DP_INPUT	0x00000011

/* CDC boost path control */
#define CDC_BOOST_PATH_ENABLE	0x00000010
#define CDC_BOOST_CFG1_DEFAULT	0x00000092
#define CDC_BOOST_CFG2_DEFAULT	0x00000008

/* DSM / RX path control */
#define RX_PATH_DSM_ENABLE	0x00000001

/* Interrupt controller pin masks */
#define INTR_CTRL_PIN_MASK_DEFAULT	0x000000F0
#define INTR_CTRL_LEVEL_DEFAULT	0x00000000
#define INTR_CTRL_BYPASS_DISABLE	0x00000000

/* RX path configuration */
#define RX_PATH_CFG1_TUNED	0x0000006E

/* Softclip */
#define SOFTCLIP_ENABLE	0x00000001

/* CB decode configuration */
#define CB_DECODE_CFG1_DEFAULT	0x00000083
#define CB_DECODE_CFG2_DEFAULT	0x00000084
#define CB_DECODE_CFG3_DEFAULT	0x00000084
#define CB_DECODE_CFG4_DEFAULT	0x000000B0
#define CB_DECODE_CFG5_DEFAULT	0x00000085
#define CB_DECODE_CFG6_DEFAULT	0x00000014
#define CB_DECODE_CFG7_DEFAULT	0x0000001E
#define CB_DECODE_CFG8_DEFAULT	0x0000000C
#define CB_DECODE_CTL_DISABLE	0x00000000
#define CB_DECODE_CTL_ENABLE	0x00000001

/* GPIO configuration */
#define TLMM_GPIO10_CFG_DEFAULT	0x00000C08
#define TLMM_GPIO11_CFG_DEFAULT	0x00000C0A

/* SWRM frame control */
#define MCP_FRAME_CTRL0_DEFAULT	0x00310000
#define MCP_FRAME_CTRL0_COMMIT	0x00310008
#define MCP_FRAME_CTRL1_DEFAULT	0x00050000
#define MCP_FRAME_CTRL1_COMMIT	0x00050008
#define MCP_FRAME_CTRL1_ENABLE	0x0005000F

/* CPUn interrupt clears */
#define CPUN_INTERRUPT_SPECIAL_DONE		0x00000004
#define CPUN_INTERRUPT_STATUS_CHANGE	0x00000002

/* SWRM reset values */
#define SWRM_COMP_SW_RESET_VALUE	0x00000001

/* SWRM CMD FIFO configuration values */
#define SWRM_CMD_FIFO_CFG_RESET	0x80000000
#define SWRM_CMD_FIFO_CFG_ENABLE	0x80000003

/* MCLK disable */
#define WSA_MCLK_DISABLE	0x00000000

/* WSA LPAIF RD DMA base addresses */
#define WSA_RDDMA_B0_BASE_ADDR	0x06C80000

/* WSA reset enable */
#define WSA_SWR_RESET_ENABLE	0x00000001

/* WSA LPAIF RD DMA buffer sizes */
#define WSA_RDDMA_BUF_LEN	0x000003FF
#define WSA_RDDMA_BUF_PER_LEN	0x000001FF

/* WSA LPAIF RD DMA control */
#define WSA_RDDMA_CTL_PREPARE	0x0020000E
#define WSA_RDDMA_CTL_ENABLE	0x0020000F

/* CODEC interface values */
#define WSA_CODEC_INTF_B0_BASE	0x00010000
#define WSA_CODEC_INTF_B0_ENABLE	0x00010003

#define WSA_CODEC_INTF_B0_FLUSH	0x80010003
#define WSA_CODEC_INTF_B0_RUN	0x40010003

/* RX path mix control */
#define RX_PATH_CTL_ENABLE_MIX	0x00000024
#define RX_PATH_MIX_ENABLE	0x00000004
#define RX_PATH_MIX_COMBINE	0x00000034
#define RX_PATH_CFG2_DEFAULT	0x0000008F

/* LPAIF IRQ clear mask */
#define LPAIF_IRQ_CLEAR_MASK_MISC	0x00048009

/* LPAIF IRQ disable */
#define LPAIF_IRQ_DISABLE	0x00000000

/* CPUn FIFO commands */
#define CPUN_CMD_FIFO_WR_STOP_B0	0x00273430
#define CPUN_CMD_FIFO_RD_STOP_B0	0x01273430

/* WSA RDDMA stop/reset */
#define WSA_RDDMA_CTL_STOP	0x8020000F
#define WSA_RDDMA_DISABLE	0x00000000
#define WSA_RDDMA_MIN_BUF_LEN	0x00000003

#define CBCR_CLK_DISABLE	BIT(0)
#define CBCR_CLK_ENABLE	BIT(0)

#define SWRM_MCP_BUS_ENABLE	BIT(0)
#define SWRM_COMP_ENABLE	BIT(0)
#define SWRM_LINK_MANAGER_ENABLE	BIT(0)

#endif  /* __DRIVERS_BUS_SOUNDWIRE_QCOM_SWRM_H__ */

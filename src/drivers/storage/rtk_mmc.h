/*
 * Copyright 2022 Realtek Inc.
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

#ifndef __DRIVERS_STORAGE_RTK_MMC_H_
#define __DRIVERS_STORAGE_RTK_MMC_H_

#include <arch/io.h>
#include <libpayload.h>

#include "drivers/gpio/gpio.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/mmc.h"

#define MAX_RW_REG_CNT		1024
#define MAX_RW_PHY_CNT		100000

#define RTK_NAME_LENGTH		32
#define RTK_VID			0x10EC
#define RTK_MMC_PID_5228	0x5228
#define RTK_MMC_PID_522A	0x522A
#define RTK_MMC_PID_525A	0x525A
#define RTK_MMC_PID_5261	0x5261

/* PCI Operation Register Address */
#define RTSX_HCBAR		0x00
#define RTSX_HCBCTLR		0x04
#define RTSX_HDBAR		0x08
#define RTSX_HDBCTLR		0x0C

/* Host Access Internal Memory Register */
#define RTSX_HAIMR		0x10
#define	HAIMR_TRANS_START	(0x01 << 31)
#define	HAIMR_READ		0x00
#define	HAIMR_WRITE		(0x01 << 30)
#define	HAIMR_READ_START	(HAIMR_TRANS_START | HAIMR_READ)
#define	HAIMR_WRITE_START	(HAIMR_TRANS_START | HAIMR_WRITE)
#define	HAIMR_TRANS_END		(HAIMR_TRANS_START)
#define HAIMR_VALID_ADDR_MASK	0x3FFF

#define RTSX_BIPR		0x14
#define RTSX_BIER		0x18
#define RTSX_DUM_REG		0x1C

/* Host command buffer control register */
#define STOP_CMD		BIT(28)

/* Host data buffer control register */
#define SDMA_MODE		0x00
#define ADMA_MODE		((u32)0x02 << 26)
#define STOP_DMA		((u32)0x01 << 28)
#define TRIG_DMA		((u32)0x01 << 31)

/* Bus interrupt pending register */
#define CMD_DONE_INT		BIT(31)
#define DATA_DONE_INT		BIT(30)
#define TRANS_OK_INT		BIT(29)
#define TRANS_FAIL_INT		BIT(28)
#define MS_INT			BIT(26)
#define SD_INT			BIT(25)
#define GPIO0_INT		BIT(24)
#define OC_INT			BIT(23)
#define SD_WRITE_PROTECT	BIT(19)
#define MS_EXIST		BIT(17)
#define SD_EXIST		BIT(16)
#define CARD_EXIST		(MS_EXIST | SD_EXIST)
#define DELINK_INT		GPIO0_INT
#define SD_OC_INT		BIT(22)
#define CARD_INT		(MS_INT | SD_INT)
#define NEED_COMPLETE_INT	(DATA_DONE_INT | TRANS_OK_INT | TRANS_FAIL_INT)
#define RTSX_INT		(CMD_DONE_INT | NEED_COMPLETE_INT | CARD_INT | GPIO0_INT | OC_INT)

/* Bus interrupt enable register */
#define CMD_DONE_INT_EN		BIT(31)
#define DATA_DONE_INT_EN	BIT(30)
#define TRANS_OK_INT_EN		BIT(29)
#define TRANS_FAIL_INT_EN	BIT(28)
#define MS_INT_EN		BIT(26)
#define SD_INT_EN		BIT(25)
#define GPIO0_INT_EN		BIT(24)
#define OC_INT_EN		BIT(23)
#define DELINK_INT_EN		GPIO0_INT_EN
#define SD_OC_INT_EN		BIT(22)

#define RTSX_RESV_BUF_LEN	4096
#define HOST_CMDS_BUF_LEN	1024
#define HOST_SG_TBL_BUF_LEN	(RTSX_RESV_BUF_LEN - HOST_CMDS_BUF_LEN)

#define SD_NR		2
#define MS_NR		3
#define SD_CARD		((u32)1 << SD_NR)
#define MS_CARD		((u32)1 << MS_NR)

#define RTK_WAIT_IC_STABLE_MS			250
#define RTK_WAIT_SD_POWER_DOWN_STABLE_MS	50
#define RTK_WAIT_SD_POWER_ON_STABLE_MS		5

enum {
	STATUS_SUCCESS	= 0x000000,
	STATUS_TIMEOUT	= 0x000001,
	STATUS_FAIL	= 0x000002,
};

enum {
	READ_REG_CMD	= 0,
	WRITE_REG_CMD	= 1,
	CHECK_REG_CMD	= 2,
};

enum {
	HOST_TO_DEVICE	= 0,
	DEVICE_TO_HOST	= 1,
};

enum {
	RTKMMC_PLATFORM_REMOVABLE		= BIT(0),
	RTKMMC_PLATFORM_NO_EMMC_HS200		= BIT(1),
	RTKMMC_PLATFORM_EMMC_1V8_POWER		= BIT(2),
	RTKMMC_PLATFORM_SUPPORTS_HS400ES	= BIT(4),
	RTKMMC_PLATFORM_SUPPORTS_HS400		= BIT(6),
	RTKMMC_PLATFORM_VALID_PRESETS		= BIT(7),
	/* The platform uses 3.3 V VCCQ (I/O signaling) */
	RTKMMC_PLATFORM_EMMC_33V_VCCQ		= BIT(8),
	/*
	 * The platform has Vcc hardwired to always be on in S0.
	 *
	 * This allows skipping a 10ms delay waiting for the eMMC power rail to
	 * stabilize after enabling bus power.
	 */
	RTK_PLATFORM_EMMC_HARDWIRED_VCC		= BIT(9),
};

enum {
	/* Control Register */
	FPDCTL				= 0xFC00,
	PDINFO				= 0xFC01,
	CLK_CTL				= 0xFC02,
	CLK_DIV				= 0xFC03,
	CLK_SEL				= 0xFC04,
	SSC_DIV_N_0			= 0xFC0F,
	SSC_DIV_N_1			= 0xFC10,
	SSC_CTL1			= 0xFC11,
	SSC_CTL2			= 0xFC12,
	RCCTL				= 0xFC14,
	FPGA_PULL_CTL			= 0xFC1D,
	OLT_LED_CTL			= 0xFC1E,
	GPIO_CTL			= 0xFC1F,
	DCM_DRP_CTL			= 0xFC23,
	DCM_DRP_TRIG			= 0xFC24,
	DCM_DRP_CFG			= 0xFC25,
	DCM_DRP_WR_DATA_L		= 0xFC26,
	DCM_DRP_WR_DATA_H		= 0xFC27,
	DCM_DRP_RD_DATA_L		= 0xFC28,
	DCM_DRP_RD_DATA_H		= 0xFC29,
	SD_VPCLK0_CTL			= 0xFC2A,
	SD_VPCLK1_CTL			= 0xFC2B,
	SD_DCMPS0_CTL			= 0xFC2C,
	SD_DCMPS1_CTL			= 0xFC2D,
	CARD_CLK_SOURCE			= 0xFC2E,
	MS_CFG				= 0xFD40,
	MS_TPC				= 0xFD41,
	MS_TRANS_CFG			= 0xFD42,
	MS_TRANSFER			= 0xFD43,
	MS_INT_REG			= 0xFD44,
	MS_BYTE_CNT			= 0xFD45,
	MS_SECTOR_CNT_L			= 0xFD46,
	MS_SECTOR_CNT_H			= 0xFD47,
	MS_DBUS_H			= 0xFD48,
	CARD_PWR_CTL			= 0xFD50,
	CARD_CLK_SWITCH			= 0xFD51,
	CARD_SHARE_MODE			= 0xFD52,
	CARD_DRIVE_SEL			= 0xFD53,
	CARD_STOP			= 0xFD54,
	CARD_OE				= 0xFD55,
	SD30_CMD_DRIVE_SEL		= 0xFD5E,
	SD30_CLK_DRIVE_SEL		= 0xFD5A,
	CARD_DATA_SOURCE		= 0xFD5B,
	CARD_SELECT			= 0xFD5C,
	SD30_DAT_DRIVE_SEL		= 0xFD5F,
	CARD_PULL_CTL1			= 0xFD60,
	CARD_PULL_CTL2			= 0xFD61,
	CARD_PULL_CTL3			= 0xFD62,
	CARD_PULL_CTL4			= 0xFD63,
	CARD_PULL_CTL5			= 0xFD64,
	CARD_PULL_CTL6			= 0xFD65,
	CARD_INT_PEND			= 0xFD68,
	CARD_CLK_EN			= 0xFD69,
	OCPCTL				= 0xFD6A,
	OCPSTAT				= 0xFD6E,
	OCPGLITCH			= 0xFD6C,
	OCPPARA1			= 0xFD6B,
	OCPPARA2			= 0xFD6D,
	REG_CRC_DUMMY_0			= 0xFD71,
	REG_DV3318_OCPCTL		= 0xFD89,
	REG_DV3318_OCPSTAT		= 0xFD8A,
	SD_CFG1				= 0xFDA0,
	SD_CFG2				= 0xFDA1,
	SD_CFG3				= 0xFDA2,
	SD_STAT1			= 0xFDA3,
	SD_STAT2			= 0xFDA4,
	SD_BUS_STAT			= 0xFDA5,
	SD_PAD_CTL			= 0xFDA6,
	SD_SAMPLE_POINT_CTL		= 0xFDA7,
	SD_PUSH_POINT_CTL		= 0xFDA8,
	SD_CMD0				= 0xFDA9,
	SD_CMD1				= 0xFDAA,
	SD_CMD2				= 0xFDAB,
	SD_CMD3				= 0xFDAC,
	SD_CMD4				= 0xFDAD,
	SD_CMD5				= 0xFDAE,
	SD_BYTE_CNT_L			= 0xFDAF,
	SD_BYTE_CNT_H			= 0xFDB0,
	SD_BLOCK_CNT_L			= 0xFDB1,
	SD_BLOCK_CNT_H			= 0xFDB2,
	SD_TRANSFER			= 0xFDB3,
	SD_CMD_STATE			= 0xFDB5,
	SD_DATA_STATE			= 0xFDB6,
	IRQEN0				= 0xFE20,
	IRQSTAT0			= 0xFE21,
	IRQEN1				= 0xFE22,
	IRQSTAT1			= 0xFE23,
	TLPRIEN				= 0xFE24,
	TLPRISTAT			= 0xFE25,
	TLPTIEN				= 0xFE26,
	TLPTISTAT			= 0xFE27,
	DMATC0				= 0xFE28,
	DMATC1				= 0xFE29,
	DMATC2				= 0xFE2A,
	DMATC3				= 0xFE2B,
	DMACTL				= 0xFE2C,
	BCTL				= 0xFE2D,
	RBBC0				= 0xFE2E,
	RBBC1				= 0xFE2F,
	RBDAT				= 0xFE30,
	RBCTL				= 0xFE34,
	CFGADDR0			= 0xFE35,
	CFGADDR1			= 0xFE36,
	CFGDATA0			= 0xFE37,
	CFGDATA1			= 0xFE38,
	CFGDATA2			= 0xFE39,
	CFGDATA3			= 0xFE3A,
	CFGRWCTL			= 0xFE3B,
	PHYRWCTL			= 0xFE3C,
	PHYDATA0			= 0xFE3D,
	PHYDATA1			= 0xFE3E,
	PHYADDR				= 0xFE3F,
	MSGRXDATA0			= 0xFE40,
	MSGRXDATA1			= 0xFE41,
	MSGRXDATA2			= 0xFE42,
	MSGRXDATA3			= 0xFE43,
	MSGTXDATA0			= 0xFE44,
	MSGTXDATA1			= 0xFE45,
	MSGTXDATA2			= 0xFE46,
	MSGTXDATA3			= 0xFE47,
	MSGTXCTL			= 0xFE48,
	LTR_CTL				= 0xFE4A,
	CDRESUMECTL			= 0xFE52,
	WAKE_SEL_CTL			= 0xFE54,
	PCLK_CTL			= 0xFE55,
	PME_FORCE_CTL			= 0xFE56,
	ASPM_FORCE_CTL			= 0xFE57,
	PM_CLK_FORCE_CTL		= 0xFE58,
	FUNC_FORCE_CTL			= 0xFE59,
	PERST_GLITCH_WIDTH		= 0xFE5C,
	CHANGE_LINK_STATE		= 0xFE5B,
	RESET_LOAD_REG			= 0xFE5E,
	HOST_SLEEP_STATE		= 0xFE60,
	PM_EVENT_DEBUG			= 0xFE71,
	NFTS_TX_CTRL			= 0xFE72,
	PWR_GATE_CTRL			= 0xFE75,
	PWD_SUSPEND_EN			= 0xFE76,
	LDO_PWR_SEL			= 0xFE78,
	L1SUB_CONFIG1			= 0xFE8D,
	L1SUB_CONFIG3			= 0xFE8F,
	REG_VREF			= 0xFE97,
	PETXCFG				= 0xFF03,
	AUTOLOAD_CFG			= 0xFF46,
	RTS5261_FW_CFG1			= 0xFF55,
	RTS5261_FW_CTL			= 0xFF5F,
	RTS5261_FPDCTL			= 0xFF60,
	LDO1233318_POW_CTL		= 0xFF70,
	LDO1_CFG0			= 0xFF72,
	LDO_VCC_CFG1			= 0xFF73,
	RTS5250_CLK_CFG3		= 0xFF79,
	AUTOLOAD_CFG1			= 0xFF7C,
	PM_CTRL3			= 0xFF7E,
	AUTOLOAD_CFG4			= 0xFF7F,
	SRAM_BASE			= 0xE600,
	RBUF_BASE			= 0xF400,
	PPBUF_BASE1			= 0xF800,
	PPBUF_BASE2			= 0xFA00,
};

enum {
	/* PHY Parameters */
	PCIE_PHY_SEL				= 0x0000,
	UHS_DPHY_SEL				= 0x4000,
	UHS_APHY_SEL				= 0x8000,
	UHS_DPHY_OFFSET				= 0x20,
	PG0_ANA01				= 0x41,
	REG_CALIB_MANUAL_MODE			= (0x1 << 1),
	PG0_ANA04				= 0x44,
	REG_POW_TERM_MAN_CTL			= (0x1 << 15),
	REG_POW_TERM_MAN			= (0x1 << 14),
	PG0_ANA0F				= 0x4f,
	PG0_ANA14				= 0x54,
	PG0_ANA1C				= 0x5C,
	DC_GAIN_SETTING				= 0x0F,
	AC_GAIN_MANUAL				= 0x04,
	PG1_ANA00				= 0x60,
	REG_AMP_MASK				= (0x7 << 13),
	PG1_ANA02				= 0x62,
	PG1_ANA04				= 0x64,
	REG_EQ_SLICER_EN			= (0x01 << 10),
	REG_EQ_DEFAULT_MASK			= (0x1F << 2),
	PG1_ANA05				= 0x65,
	REG_RX_EQ_ACGAIN_SEL_LN0		= (0x1 << 15),
	PG1_ANA06				= 0x66,
	PG1_ANA08				= 0x68,
	REG_LN0_RX_EQ_ACGAIN_MASK_VC		= (0x1F << 11),
	PG1_ANA09				= 0x69,
	PG1_ANA0A				= 0x6a,
	REG_RX_EQ_ACGAIN_SEL_LN1		= (0x1 << 15),
	REG_RXAMP_RLSEL_MASK			= (0x3 << 4),
	REG_RXAMP_KOFF_RANGE_MASK		= (0x3 << 1),
	PG1_ANA0B				= 0x6b,
	REG_LN1_RX_EQ_ACGAIN_MASK_VC		= (0x1F << 11),
	PG1_ANA0F				= 0x6f,
	PG1_ANA10				= 0x70,
	REG_TX_SLEW_RATE_MASK			= (0x7 << 13),
	PG1_ANA11				= 0x71,
	REG_VREF_SEL_LDO_RX_MASK		= (0x7 << 5),
	PG1_ANA13				= 0x73,
	PG1_ANA14				= 0x74,
	PG1_ANA16				= 0x76,
	REG_CDR_R_MASK				= (0x3F << 0),
	REG_TXCMU_VSEL_LDOA_MASK		= (0x7 << 13),
	REG_LEQ_DCGAIN_LN0			= (0x1F << 11),
	REG_LEQ_DCGAIN_LN1			= (0x1F << 6),
	PG1_ANA15				= 0x75,
	PG1_ANA17				= 0x77,
	PG1_ANA18				= 0x78,
	PG1_ANA19				= 0x79,
	PG1_ANA1A				= 0x7a,
	PG1_ANA1B				= 0x7b,
	PG1_ANA1C				= 0x7c,
	PG1_ANA1D				= 0x7d,
	PG1_ANA1E				= 0x7e,
	PG1_ANA1F				= 0x7f,
	PG2_ANA1C				= 0x9c,
	PG2_ANA1E				= 0x9E,
	PG2_ANA1F				= 0x9F,
	REG_LEQ_ACGAIN_MLSB_SEL			= (0x1 << 6),
	PHY_DEBUG_MODE				= 0x01,
	PHY_CFG_RW_OK				= (0x1 << 7),
	PHY_TUNE				= 0x08,
	PHY_TUNE_SDBUS_33			= 0x0200,
};

enum {
	/* Power parameters */
	SSC_POWER_DOWN			= 0x01,
	SD_OC_POWER_DOWN		= 0x02,
	ALL_POWER_DOWN			= 0x03,
	OC_POWER_DOWN			= 0x02,
	PMOS_STRG_MASK			= 0x10,
	PMOS_STRG_800mA			= 0x10,
	PMOS_STRG_400mA			= 0x00,
	POWER_OFF			= 0x03,
	PARTIAL_POWER_ON		= 0x01,
	POWER_ON			= 0x00,
	MS_POWER_OFF			= 0x0C,
	MS_PARTIAL_POWER_ON		= 0x04,
	MS_POWER_ON			= 0x00,
	MS_POWER_MASK			= 0x0C,
	SD_POWER_OFF			= 0x03,
	SD_PARTIAL_POWER_ON		= 0x01,
	SD_POWER_ON			= 0x00,
	SD_POWER_MASK			= 0x03,
	SD_OUTPUT_EN			= 0x04,
	MS_OUTPUT_EN			= 0x08,
	PWR_GATE_EN			= 0x01,
	LDO3318_PWR_MASK		= 0x06,
	LDO_ON				= 0x06,
	LDO_SUSPEND			= 0x02,
	LDO_OFF				= 0x00,
	LDO_VCC_REF_TUNE_MASK		= 0x30,
	LDO_VCC_REF_1V2			= 0x20,
	LDO_VCC_TUNE_MASK		= 0x07,
	LDO_VCC_1V8			= 0x04,
	LDO_VCC_3V3			= 0x07,
	RTS5228_RTS5261_LDO_VCC_TUNE_MASK	= (0x7 << 1),
	RTS5228_RTS5261_LDO_VCC_3V3	= (0x7 << 1),
	LDO_VCC_LMT_EN			= 0x08,
	LDO_SRC_SEL_MASK		= 0x03,
	LDO_SRC_NONE			= 0x00,
	LDO_SRC_PMOS			= 0x01,
	LDO_POWERON_MASK		= 0x0F,
	LDO1_POWERON_MASK		= 0x03,
	LDO1_POWEROFF			= 0x00,
	LDO1_SOFTSTART			= 0x01,
	LDO1_FULLON			= 0x03,
};

enum {
	/* SSC parameters */
	SSC_80				= 0,
	SSC_100				= 1,
	SSC_120				= 2,
	SSC_150				= 3,
	SSC_200				= 4,
	SSC_RSTB			= 0x80,
	SSC_8X_EN			= 0x40,
	SSC_FIX_FRAC			= 0x20,
	SSC_SEL_1M			= 0x00,
	SSC_SEL_2M			= 0x08,
	SSC_SEL_4M			= 0x10,
	SSC_SEL_8M			= 0x18,
	SSC_DEPTH_MASK			= 0x07,
	SSC_DEPTH_DISALBE		= 0x00,
	SSC_DEPTH_4M			= 0x01,
	SSC_DEPTH_2M			= 0x02,
	SSC_DEPTH_1M			= 0x03,
	SSC_DEPTH_512K			= 0x04,
	SSC_DEPTH_256K			= 0x05,
	SSC_DEPTH_128K			= 0x06,
	SSC_DEPTH_64K			= 0x07,
	RTS5228_RTS5261_SSC_DEPTH_8M	= 0x01,
	RTS5228_RTS5261_SSC_DEPTH_4M	= 0x02,
	RTS5228_RTS5261_SSC_DEPTH_2M	= 0x03,
	RTS5228_RTS5261_SSC_DEPTH_1M	= 0x04,
	RTS5228_RTS5261_SSC_DEPTH_512K	= 0x05,
};

enum {
	/* Card Clock parameters*/
	CLK_LOW_FREQ			= 0x01,
	CLK_DIV_1			= 0x01,
	CLK_DIV_2			= 0x02,
	CLK_DIV_4			= 0x03,
	CLK_DIV_8			= 0x04,
	SD_CLK_EN			= 0x04,
	MS_CLK_EN			= 0x08,
	CHANGE_CLK			= 0x01,
	SD_CLK_DIVIDE_MASK		= 0xC0,
	SD_CLK_DIVIDE_0			= 0x00,
	SD_CLK_DIVIDE_256		= 0xC0,
	SD_CLK_DIVIDE_128		= 0x80,
	FORCE_PM_CLOCK			= 0x10,
	EN_CLOCK_PM			= 0x01,
	FORCE_CLKREQ_DELINK_MASK	= 0x80,
	FORCE_CLKREQ_LOW		= 0x80,
	FORCE_CLKREQ_HIGH		= 0x00,
	PCLK_MODE_SEL			= 0x20,
};

enum {
	/* SD card parameters */
	SD_MOD_SEL			= 2,
	MS_MOD_SEL			= 3,
	SD_CRC7_ERR			= 0x80,
	SD_CRC16_ERR			= 0x40,
	SD_CRC_WRITE_ERR		= 0x20,
	SD_CRC_WRITE_ERR_MASK		= 0x1C,
	GET_CRC_TIME_OUT		= 0x02,
	SD_TUNING_COMPARE_ERR		= 0x01,
	SD_RSP_80CLK_TIMEOUT		= 0x01,
	SD_CLK_TOGGLE_EN		= 0x80,
	SD_CLK_FORCE_STOP		= 0x40,
	SD_DAT3_STATUS			= 0x10,
	SD_DAT2_STATUS			= 0x08,
	SD_DAT1_STATUS			= 0x04,
	SD_DAT0_STATUS			= 0x02,
	SD_CMD_STATUS			= 0x01,
	SD_IO_USING_1V8			= 0x80,
	SD_IO_USING_3V3			= 0x7F,
	TYPE_A_DRIVING			= 0x00,
	TYPE_B_DRIVING			= 0x01,
	TYPE_C_DRIVING			= 0x02,
	TYPE_D_DRIVING			= 0x03,
	DDR_FIX_RX_DAT			= 0x00,
	DDR_VAR_RX_DAT			= 0x80,
	DDR_FIX_RX_DAT_EDGE		= 0x00,
	DDR_FIX_RX_DAT_14_DELAY		= 0x40,
	DDR_FIX_RX_CMD			= 0x00,
	DDR_VAR_RX_CMD			= 0x20,
	DDR_FIX_RX_CMD_POS_EDGE		= 0x00,
	DDR_FIX_RX_CMD_14_DELAY		= 0x10,
	SD20_RX_POS_EDGE		= 0x00,
	SD20_RX_14_DELAY		= 0x08,
	SD20_RX_SEL_MASK		= 0x08,
	DDR_FIX_TX_CMD_DAT		= 0x00,
	DDR_VAR_TX_CMD_DAT		= 0x80,
	DDR_FIX_TX_DAT_14_TSU		= 0x00,
	DDR_FIX_TX_DAT_12_TSU		= 0x40,
	DDR_FIX_TX_CMD_NEG_EDGE		= 0x00,
	DDR_FIX_TX_CMD_14_AHEAD		= 0x20,
	SD20_TX_NEG_EDGE		= 0x00,
	SD20_TX_14_AHEAD		= 0x10,
	SD20_TX_SEL_MASK		= 0x10,
	DDR_VAR_SDCLK_POL_SWAP		= 0x01,
	SD_TRANSFER_START		= 0x80,
	SD_TRANSFER_END			= 0x40,
	SD_STAT_IDLE			= 0x20,
	SD_TRANSFER_ERR			= 0x10,
	SD_TM_NORMAL_WRITE		= 0x00,
	SD_TM_AUTO_WRITE_3		= 0x01,
	SD_TM_AUTO_WRITE_4		= 0x02,
	SD_TM_AUTO_READ_3		= 0x05,
	SD_TM_AUTO_READ_4		= 0x06,
	SD_TM_CMD_RSP			= 0x08,
	SD_TM_AUTO_WRITE_1		= 0x09,
	SD_TM_AUTO_WRITE_2		= 0x0A,
	SD_TM_NORMAL_READ		= 0x0C,
	SD_TM_AUTO_READ_1		= 0x0D,
	SD_TM_AUTO_READ_2		= 0x0E,
	SD_TM_AUTO_TUNING		= 0x0F,
	PHASE_CHANGE			= 0x80,
	PHASE_NOT_RESET			= 0x40,
	DCMPS_CHANGE			= 0x80,
	DCMPS_CHANGE_DONE		= 0x40,
	DCMPS_ERROR			= 0x20,
	DCMPS_CURRENT_PHASE		= 0x1F,
	SD_BUS_WIDTH_1			= 0x00,
	SD_BUS_WIDTH_4			= 0x01,
	SD_BUS_WIDTH_8			= 0x02,
	SD_ASYNC_FIFO_NOT_RST		= 0x10,
	SD_20_MODE			= 0x00,
	SD_DDR_MODE			= 0x04,
	SD_30_MODE			= 0x08,
	SD_CMD_IDLE			= 0x80,
	SD_DATA_IDLE			= 0x80,
	DCM_RESET			= 0x08,
	DCM_LOCKED			= 0x04,
	DCM_208M			= 0x00,
	DCM_TX				= 0x01,
	DCM_RX				= 0x02,
	DRP_START			= 0x80,
	DRP_DONE			= 0x40,
	DRP_WRITE			= 0x80,
	DRP_READ			= 0x00,
	DCM_WRITE_ADDRESS_50		= 0x50,
	DCM_WRITE_ADDRESS_51		= 0x51,
	DCM_READ_ADDRESS_00		= 0x00,
	DCM_READ_ADDRESS_51		= 0x51,
	SD_CALCULATE_CRC7		= 0x00,
	SD_NO_CALCULATE_CRC7		= 0x80,
	SD_CHECK_CRC16			= 0x00,
	SD_NO_CHECK_CRC16		= 0x40,
	SD_NO_CHECK_WAIT_CRC_TO		= 0x20,
	SD_WAIT_BUSY_END		= 0x08,
	SD_NO_WAIT_BUSY_END		= 0x00,
	SD_CHECK_CRC7			= 0x00,
	SD_NO_CHECK_CRC7		= 0x04,
	SD_RSP_LEN_0			= 0x00,
	SD_RSP_LEN_6			= 0x01,
	SD_RSP_LEN_17			= 0x02,
	SD_RSP_TYPE_R0			= 0x04,
	SD_RSP_TYPE_R1			= 0x01,
	SD_RSP_TYPE_R1b			= 0x09,
	SD_RSP_TYPE_R2			= 0x02,
	SD_RSP_TYPE_R3			= 0x05,
	SD_RSP_TYPE_R4			= 0x05,
	SD_RSP_TYPE_R5			= 0x01,
	SD_RSP_TYPE_R6			= 0x01,
	SD_RSP_TYPE_R7			= 0x01,
	SD_RSP_80CLK_TIMEOUT_EN		= 0x01,
	SAMPLE_TIME_RISING		= 0x00,
	SAMPLE_TIME_FALLING		= 0x80,
	PUSH_TIME_DEFAULT		= 0x00,
	PUSH_TIME_ODD			= 0x40,
	NO_EXTEND_TOGGLE		= 0x00,
	EXTEND_TOGGLE_CHK		= 0x20,
	MS_BUS_WIDTH_1			= 0x00,
	MS_BUS_WIDTH_4			= 0x10,
	MS_BUS_WIDTH_8			= 0x18,
	MS_2K_SECTOR_MODE		= 0x04,
	MS_512_SECTOR_MODE		= 0x00,
	MS_TOGGLE_TIMEOUT_EN		= 0x00,
	MS_TOGGLE_TIMEOUT_DISEN		= 0x01,
	MS_NO_CHECK_INT			= 0x02,
	WAIT_INT			= 0x80,
	NO_WAIT_INT			= 0x00,
	NO_AUTO_READ_INT_REG		= 0x00,
	AUTO_READ_INT_REG		= 0x40,
	MS_CRC16_ERR			= 0x20,
	MS_RDY_TIMEOUT			= 0x10,
	MS_INT_CMDNK			= 0x08,
	MS_INT_BREQ			= 0x04,
	MS_INT_ERR			= 0x02,
	MS_INT_CED			= 0x01,
	MS_TRANSFER_START		= 0x80,
	MS_TRANSFER_END			= 0x40,
	MS_TRANSFER_ERR			= 0x20,
	MS_BS_STATE			= 0x10,
	MS_TM_READ_BYTES		= 0x00,
	MS_TM_NORMAL_READ		= 0x01,
	MS_TM_WRITE_BYTES		= 0x04,
	MS_TM_NORMAL_WRITE		= 0x05,
	MS_TM_AUTO_READ			= 0x08,
	MS_TM_AUTO_WRITE		= 0x0C,
	CARD_SHARE_MASK			= 0x0F,
	CARD_SHARE_NORMAL		= 0x00,
	CARD_SHARE_SD			= 0x04,
	CARD_SHARE_MS			= 0x08,
	SD_STOP				= 0x04,
	MS_STOP				= 0x08,
	SD_CLR_ERR			= 0x40,
	MS_CLR_ERR			= 0x80,
	CRC_FIX_CLK			= (0x00 << 0),
	CRC_VAR_CLK0			= (0x01 << 0),
	CRC_VAR_CLK1			= (0x02 << 0),
	SD30_FIX_CLK			= (0x00 << 2),
	SD30_VAR_CLK0			= (0x01 << 2),
	SD30_VAR_CLK1			= (0x02 << 2),
	SAMPLE_FIX_CLK			= (0x00 << 4),
	SAMPLE_VAR_CLK0			= (0x01 << 4),
	SAMPLE_VAR_CLK1			= (0x02 << 4),
	PINGPONG_BUFFER			= 0x01,
	RING_BUFFER			= 0x00,
	RB_FLUSH			= 0x80,
	RB_FULL				= 0x02,
	MS_DRIVE_8mA			= (0x01 << 6),
	MMC_DRIVE_8mA			= (0x01 << 4),
	XD_DRIVE_8mA			= (0x01 << 2),
	GPIO_DRIVE_8mA			= 0x01,
	RTSX_CARD_DRIVE_DEFAULT		= (MS_DRIVE_8mA | GPIO_DRIVE_8mA),
	PWD_SUSPND_EN			= 0x10,
	RTS525A_CFG_MEM_PD		= 0xF0,
	PME_DEBUG_0			= 0x08,
	CD_RESUME_EN_MASK		= 0xF0,
	AUX_CLK_ACTIVE_SEL_MASK		= 0x01,
	MAC_CKSW_DONE			= 0x00,
	RTS5261_INFORM_RTD3_COLD	= (0x01 << 5),
	RTS5261_FORCE_PRSNT_LOW		= (0x01 << 6),
	RTS5261_AUX_CLK_16M_EN		= (0x01 << 5),
	RTS5261_MCU_CLOCK_GATING	= (0x01 << 1),
};

enum {
	/* PCI Express Related Registers */
	DMA_DONE_INT_EN			= 0x80,
	SUSPEND_INT_EN			= 0x40,
	LINK_RDY_INT_EN			= 0x20,
	LINK_DOWN_INT_EN		= 0x10,
	DMA_DONE_INT			= 0x80,
	SUSPEND_INT			= 0x40,
	LINK_RDY_INT			= 0x20,
	LINK_DOWN_INT			= 0x10,
	MRD_ERR_INT_EN			= 0x40,
	MWR_ERR_INT_EN			= 0x20,
	SCSI_CMD_INT_EN			= 0x10,
	TLP_RCV_INT_EN			= 0x08,
	TLP_TRSMT_INT_EN		= 0x04,
	MRD_COMPLETE_INT_EN		= 0x02,
	MWR_COMPLETE_INT_EN		= 0x01,
	MRD_ERR_INT			= 0x40,
	MWR_ERR_INT			= 0x20,
	SCSI_CMD_INT			= 0x10,
	TLP_RX_INT			= 0x08,
	TLP_TX_INT			= 0x04,
	MRD_COMPLETE_INT		= 0x02,
	MWR_COMPLETE_INT		= 0x01,
	MSG_RX_INT_EN			= 0x08,
	MRD_RX_INT_EN			= 0x04,
	MWR_RX_INT_EN			= 0x02,
	CPLD_RX_INT_EN			= 0x01,
	MSG_RX_INT			= 0x08,
	MRD_RX_INT			= 0x04,
	MWR_RX_INT			= 0x02,
	CPLD_RX_INT			= 0x01,
	MSG_TX_INT_EN			= 0x08,
	MRD_TX_INT_EN			= 0x04,
	MWR_TX_INT_EN			= 0x02,
	CPLD_TX_INT_EN			= 0x01,
	MSG_TX_INT			= 0x08,
	MRD_TX_INT			= 0x04,
	MWR_TX_INT			= 0x02,
	CPLD_TX_INT			= 0x01,
	DMA_RST				= 0x80,
	DMA_BUSY			= 0x04,
	DMA_DIR_TO_CARD			= 0x00,
	DMA_DIR_FROM_CARD		= 0x02,
	DMA_EN				= 0x01,
	DMA_128				= (0x00 << 4),
	DMA_256				= (0x01 << 4),
	DMA_512				= (0x02 << 4),
	DMA_1024			= (0x03 << 4),
	DMA_PACK_SIZE_MASK		= 0x30,
};

enum {
	/* OCP Parameters */
	SD_DETECT_EN			= 0x08,
	SD_OCP_INT_EN			= 0x04,
	SD_OCP_INT_CLR			= 0x02,
	SD_OC_CLR			= 0x01,
	SD_OCP_DETECT			= 0x08,
	SD_OC_NOW			= 0x04,
	SD_OC_EVER			= 0x02,
	SD_OCP_GLITCH_MASK		= 0x0F,
	SD_OCP_GLITCH_NONE		= 0x00,
	SD_OCP_GLITCH_50U		= 0x01,
	SD_OCP_GLITCH_100U		= 0x02,
	SD_OCP_GLITCH_200U		= 0x03,
	SD_OCP_GLITCH_600U		= 0x04,
	SD_OCP_GLITCH_800U		= 0x05,
	SD_OCP_GLITCH_1M		= 0x06,
	SD_OCP_GLITCH_2M		= 0x07,
	SD_OCP_GLITCH_3M		= 0x08,
	SD_OCP_GLITCH_4M		= 0x09,
	SD_OCP_GLIVCH_5M		= 0x0A,
	SD_OCP_GLITCH_6M		= 0x0B,
	SD_OCP_GLITCH_7M		= 0x0C,
	SD_OCP_GLITCH_8M		= 0x0D,
	SD_OCP_GLITCH_9M		= 0x0E,
	SD_OCP_GLITCH_10M		= 0x0F,
	SD_OCP_TIME_60			= 0x00,
	SD_OCP_TIME_100			= 0x01,
	SD_OCP_TIME_200			= 0x02,
	SD_OCP_TIME_400			= 0x03,
	SD_OCP_TIME_600			= 0x04,
	SD_OCP_TIME_800			= 0x05,
	SD_OCP_TIME_1100		= 0x06,
	SD_OCP_TIME_MASK		= 0x07,
	SD_OCP_THD_450			= 0x00,
	SD_OCP_THD_550			= 0x01,
	SD_OCP_THD_650			= 0x02,
	SD_OCP_THD_750			= 0x03,
	SD_OCP_THD_850			= 0x04,
	SD_OCP_THD_950			= 0x05,
	SD_OCP_THD_1050			= 0x06,
	SD_OCP_THD_1150			= 0x07,
	SD_OCP_THD_MASK			= 0x07,
	DV3318_OCP_TIME_MASK		= 0xF0,
	DV3318_DETECT_EN		= 0x08,
	DV3318_OCP_INT_EN		= 0x04,
	DV3318_OCP_INT_CLR		= 0x02,
	DV3318_OCP_CLR			= 0x01,
	DV3318_OCP_GlITCH_MASK		= 0xF0,
	DV3318_OCP_DETECT		= 0x08,
	DV3318_OCP_NOW			= 0x04,
	DV3318_OCP_EVER			= 0x02,
	LDO1_OCP_EN			= (0x01 << 4),
	LDO1_OCP_LMT_EN			= (0x01 << 1),
	LDO1_OCP_THD_MASK		= (0x07 << 5),
	LDO1_OCP_LMT_THD_MASK		= (0x03 << 2),
};

enum {
	/* Pad Pull parameters */
	SD_D7_NP		= 0x00,
	SD_D7_PD		= (0x01 << 4),
	SD_D7_PU		= (0x02 << 4),
	SD_D6_NP		= 0x00,
	SD_D6_PD		= (0x01 << 6),
	SD_D6_PU		= (0x02 << 6),
	SD_D5_NP		= 0x00,
	SD_D5_PD		= 0x01,
	SD_D5_PU		= 0x02,
	SD_D4_NP		= 0x00,
	SD_D4_PD		= (0x01 << 6),
	SD_D4_PU		= (0x02 << 6),
	SD_D3_NP		= 0x00,
	SD_D3_PD		= (0x01 << 4),
	SD_D3_PU		= (0x02 << 4),
	SD_D2_NP		= 0x00,
	SD_D2_PD		= (0x01 << 2),
	SD_D2_PU		= (0x02 << 2),
	SD_D1_NP		= 0x00,
	SD_D1_PD		= 0x01,
	SD_D1_PU		= 0x02,
	SD_D0_NP		= 0x00,
	SD_D0_PD		= (0x01 << 4),
	SD_D0_PU		= (0x02 << 4),
	SD_WP_NP		= 0x00,
	SD_WP_PD		= (0x01 << 5),
	SD_WP_PU		= (0x02 << 5),
	SD_CD_PD		= 0x00,
	SD_CD_PU		= (0x01 << 4),
	SD_CMD_NP		= 0x00,
	SD_CMD_PD		= (0x01 << 2),
	SD_CMD_PU		= (0x02 << 2),
	SD_CLK_NP		= 0x00,
	SD_CLK_PD		= (0x01 << 2),
	SD_CLK_PU		= (0x02 << 2),
	MS_D7_NP		= 0x00,
	MS_D7_PD		= (0x01 << 6),
	MS_D7_PU		= (0x02 << 6),
	MS_D6_NP		= 0x00,
	MS_D6_PD		= 0x01,
	MS_D6_PU		= 0x02,
	MS_D5_NP		= 0x00,
	MS_D5_PD		= (0x01 << 2),
	MS_D5_PU		= (0x02 << 2),
	MS_D4_NP		= 0x00,
	MS_D4_PD		= 0x01,
	MS_D4_PU		= 0x02,
	MS_D3_NP		= 0x00,
	MS_D3_PD		= (0x01 << 6),
	MS_D3_PU		= (0x02 << 6),
	MS_D2_NP		= 0x00,
	MS_D2_PD		= (0x01 << 4),
	MS_D2_PU		= (0x02 << 4),
	MS_D1_NP		= 0x00,
	MS_D1_PD		= (0x01 << 6),
	MS_D1_PU		= (0x02 << 6),
	MS_D0_NP		= 0x00,
	MS_D0_PD		= (0x01 << 4),
	MS_D0_PU		= (0x02 << 4),
	MS_CLK_NP		= 0x00,
	MS_CLK_PD		= (0x01 << 2),
	MS_CLK_PU		= (0x02 << 2),
	MS_BS_NP		= 0x00,
	MS_BS_PD		= (0x01 << 2),
	MS_BS_PU		= (0x02 << 2),
	MS_INS_PD		= 0x00,
	MS_INS_PU		= (0x01 << 7),
};

typedef struct {
	MmcCtrlr mmc_ctrlr;
	pcidev_t dev;

	char *name;
	void *ioaddr;
	int detection_tries;
	unsigned platform_info;
	int initialized;

	u16 pid;
	u8 ic_version;
	u32 sd_addr;
	u32 int_reg;	/* Bus interrupt register */
	void *host_cmds_ptr;/* Commands buffer pointer */
	int ci;		/* Command Index */
	u8 bus_width;
	enum mmc_timing timing;/* current timing */

	u8 ssc_depth;
	bool vpclk;
	bool double_clk;
	bool initial_mode;
	unsigned int clock;
	unsigned int cur_clock;

	u8 ssc_depth_sd_sdr104;
	u8 ssc_depth_sd_ddr50;
	u8 ssc_depth_sd_sdr50;
	u8 ssc_depth_sd_hs;
	u8 ssc_depth_mmc_52m;
	u8 ssc_depth_ms_hg;
	u8 ssc_depth_ms_4bit;
	u8 ssc_depth_low_speed;

	u8 card_drive_sel;
	u8 sd30_drive_sel_1v8;
	u8 sd30_drive_sel_3v3;

	u8 sd30_clk_drive_sel_1v8;
	u8 sd30_cmd_drive_sel_1v8;
	u8 sd30_dat_drive_sel_1v8;
	u8 sd30_clk_drive_sel_3v3;
	u8 sd30_cmd_drive_sel_3v3;
	u8 sd30_dat_drive_sel_3v3;

	u8 sd_400mA_ocp_thd;
	u8 sd_800mA_ocp_thd;
	u8 sd_ocp_lmt_thd;

} RtkMmcHost;

static inline void rtsx_writel(RtkMmcHost *host, u32 reg, u32 val)
{
	write32(host->ioaddr + reg, val);
}

static inline u32 rtsx_readl(RtkMmcHost *host, u32 reg)
{
	return read32(host->ioaddr + reg);
}

int rtsx_write_register(RtkMmcHost *host, u16 addr, u8 mask, u8 data);
int rtsx_read_register(RtkMmcHost *host, u16 addr, u8 *data);

#define CHK_PCI_PID(host, ic)		((host)->pid == (ic))
#define RTSX_WRITE_REG(host, addr, mask, data)				\
	do {								\
		int retval = rtsx_write_register((host), (addr),	\
			(mask), (data));				\
		if (retval != STATUS_SUCCESS)				\
			return retval;					\
	} while (0)

#define RTSX_READ_REG(host, addr, data)	\
	do {								\
		int retval = rtsx_read_register((host), (addr), (data));\
		if (retval != STATUS_SUCCESS)				\
			return retval;											\
	} while (0)

RtkMmcHost *probe_pci_rtk_host(pcidev_t dev, unsigned int platform_info);

#endif /* __DRIVERS_STORAGE_RTK_MMC_H_ */

/*
 * Copyright 2015 MediaTek Inc.
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

#ifndef __DRIVERS_STORAGE_MTK_MMC_PRIVATE_H_
#define __DRIVERS_STORAGE_MTK_MMC_PRIVATE_H_
enum {
	RESP_NONE = 0,
	RESP_R1 = 1,
	RESP_R2 = 2,
	RESP_R3 = 3,
	RESP_R4 = 4,
	RESP_R5 = 1,
	RESP_R6 = 1,
	RESP_R7 = 1,
	RESP_R1B = 7
};

/*--------------------------------------------------------------------------*/
/* Register Mask                                                            */
/*--------------------------------------------------------------------------*/

enum {
	/* MSDC_CFG mask */
	MSDC_CFG_MODE		= (0x1  << 0),	/* RW */
	MSDC_CFG_CKPDN		= (0x1  << 1),	/* RW */
	MSDC_CFG_RST		= (0x1  << 2),	/* RW */
	MSDC_CFG_PIO		= (0x1  << 3),	/* RW */
	MSDC_CFG_CKDRVEN	= (0x1  << 4),	/* RW */
	MSDC_CFG_BV18SDT	= (0x1  << 5),	/* RW */
	MSDC_CFG_BV18PSS	= (0x1  << 6),	/* R  */
	MSDC_CFG_CKSTB		= (0x1  << 7),	/* R  */
};

enum {
	/* MSDC_IOCON mask */
	MSDC_IOCON_SDR104CKS	= (0x1  << 0),	/* RW */
	MSDC_IOCON_RSPL		= (0x1  << 1),	/* RW */
	MSDC_IOCON_DSPL		= (0x1  << 2),	/* RW */
	MSDC_IOCON_DDLSEL	= (0x1  << 3),	/* RW */
	MSDC_IOCON_DDR50CKD	= (0x1  << 4),	/* RW */
	MSDC_IOCON_DSPLSEL	= (0x1  << 5),	/* RW */
	MSDC_IOCON_W_DSPL	= (0x1  << 8),	/* RW */
	MSDC_IOCON_D0SPL	= (0x1  << 16),	/* RW */
	MSDC_IOCON_D1SPL	= (0x1  << 17),	/* RW */
	MSDC_IOCON_D2SPL	= (0x1  << 18),	/* RW */
	MSDC_IOCON_D3SPL	= (0x1  << 19),	/* RW */
	MSDC_IOCON_D4SPL	= (0x1  << 20),	/* RW */
	MSDC_IOCON_D5SPL	= (0x1  << 21),	/* RW */
	MSDC_IOCON_D6SPL	= (0x1  << 22),	/* RW */
	MSDC_IOCON_D7SPL	= (0x1  << 23),	/* RW */
	MSDC_IOCON_RISCSZ	= (0x3  << 24),	/* RW */
};

enum {
	/* MSDC_PS mask */
	MSDC_PS_CDEN		= (0x1  << 0),	/* RW */
	MSDC_PS_CDSTS		= (0x1  << 1),	/* R  */
	MSDC_PS_CDDEBOUNCE	= (0xf  << 12),	/* RW */
	MSDC_PS_DAT		= (0xff << 16),	/* R  */
	MSDC_PS_CMD		= (0x1  << 24),	/* R  */
	MSDC_PS_WP		= (0x1UL << 31),	/* R  */
};

enum {
	/* MSDC_INT mask */
	MSDC_INT_MMCIRQ		= (0x1  << 0),	/* W1C */
	MSDC_INT_CDSC		= (0x1  << 1),	/* W1C */
	MSDC_INT_ACMDRDY	= (0x1  << 3),	/* W1C */
	MSDC_INT_ACMDTMO	= (0x1  << 4),	/* W1C */
	MSDC_INT_ACMDCRCERR	= (0x1  << 5),	/* W1C */
	MSDC_INT_DMAQ_EMPTY	= (0x1  << 6),	/* W1C */
	MSDC_INT_SDIOIRQ	= (0x1  << 7),	/* W1C */
	MSDC_INT_CMDRDY		= (0x1  << 8),	/* W1C */
	MSDC_INT_CMDTMO		= (0x1  << 9),	/* W1C */
	MSDC_INT_RSPCRCERR	= (0x1  << 10),	/* W1C */
	MSDC_INT_CSTA		= (0x1  << 11),	/* R */
	MSDC_INT_XFER_COMPL	= (0x1  << 12),	/* W1C */
	MSDC_INT_DXFER_DONE	= (0x1  << 13),	/* W1C */
	MSDC_INT_DATTMO		= (0x1  << 14),	/* W1C */
	MSDC_INT_DATCRCERR	= (0x1  << 15),	/* W1C */
	MSDC_INT_ACMD19_DONE	= (0x1  << 16),	/* W1C */
	MSDC_INT_DMA_BDCSERR	= (0x1  << 17),	/* W1C */
	MSDC_INT_DMA_GPDCSERR	= (0x1  << 18),	/* W1C */
	MSDC_INT_DMA_PROTECT	= (0x1  << 19),	/* W1C */
};

enum {
	/* MSDC_FIFOCS mask */
	MSDC_FIFOCS_RXCNT	= (0xff << 0),	/* R */
	MSDC_FIFOCS_TXCNT	= (0xff << 16),	/* R */
	MSDC_FIFOCS_CLR		= (0x1UL << 31),	/* RW */
};

enum {
	/* SDC_CFG mask */
	SDC_CFG_SDIOINTWKUP	= (0x1  << 0),	/* RW */
	SDC_CFG_INSWKUP		= (0x1  << 1),	/* RW */
	SDC_CFG_BUSWIDTH	= (0x3  << 16),	/* RW */
	SDC_CFG_SDIO		= (0x1  << 19),	/* RW */
	SDC_CFG_SDIOIDE		= (0x1  << 20),	/* RW */
	SDC_CFG_INTATGAP	= (0x1  << 21),	/* RW */
	SDC_CFG_DTOC		= (0xffUL << 24),	/* RW */
};

enum {
	/* SDC_CMD mask */
	SDC_CMD_OPC		= (0x3f << 0),	/* RW */
	SDC_CMD_BRK		= (0x1  << 6),	/* RW */
	SDC_CMD_RSPTYP		= (0x7  << 7),	/* RW */
	SDC_CMD_DTYP		= (0x3  << 11),	/* RW */
	SDC_CMD_RW		= (0x1  << 13),	/* RW */
	SDC_CMD_STOP		= (0x1  << 14),	/* RW */
	SDC_CMD_GOIRQ		= (0x1  << 15),	/* RW */
	SDC_CMD_BLKLEN		= (0xfff << 16),	/* RW */
	SDC_CMD_AUTOCMD		= (0x3  << 28),	/* RW */
	SDC_CMD_VOLSWTH		= (0x1  << 30),	/* RW */
};

enum {
	/* SDC_STS mask */
	SDC_STS_SDCBUSY		= (0x1  << 0),	/* RW */
	SDC_STS_CMDBUSY		= (0x1  << 1),	/* RW */
	SDC_STS_SWR_COMPL	= (0x1  << 31),	/* RW */
};

enum {
	/* SDC_DCRC_STS mask */
	SDC_DCRC_STS_NEG	= (0xff << 8),	/* RO */
	SDC_DCRC_STS_POS	= (0xff << 0),	/* RO */
};

enum {
	/* SDC_ADV_CFG0 mask */
	SDC_RX_ENHANCE_EN	= (0x1 << 20),	/* RW */
};

enum {
	/* EMMC_CFG0 mask */
	EMMC_CFG0_BOOTSTART	= (0x1  << 0),	/* W */
	EMMC_CFG0_BOOTSTOP	= (0x1  << 1),	/* W */
	EMMC_CFG0_BOOTMODE	= (0x1  << 2),	/* RW */
	EMMC_CFG0_BOOTACKDIS	= (0x1  << 3),	/* RW */
	EMMC_CFG0_BOOTWDLY	= (0x7  << 12),	/* RW */
	EMMC_CFG0_BOOTSUPP	= (0x1  << 15),	/* RW */
};

enum {
	/* EMMC_CFG1 mask */
	EMMC_CFG1_BOOTDATTMC	= (0xfffff << 0),	/* RW */
	EMMC_CFG1_BOOTACKTMC	= (0xfffUL << 20),	/* RW */
};

enum {
	/* EMMC_STS mask */
	EMMC_STS_BOOTCRCERR	= (0x1  << 0),	/* W1C */
	EMMC_STS_BOOTACKERR	= (0x1  << 1),	/* W1C */
	EMMC_STS_BOOTDATTMO	= (0x1  << 2),	/* W1C */
	EMMC_STS_BOOTACKTMO	= (0x1  << 3),	/* W1C */
	EMMC_STS_BOOTUPSTATE	= (0x1  << 4),	/* R */
	EMMC_STS_BOOTACKRCV	= (0x1  << 5),	/* W1C */
	EMMC_STS_BOOTDATRCV	= (0x1  << 6),	/* R */
};

enum {
	/* EMMC_IOCON mask */
	EMMC_IOCON_BOOTRST	= (0x1  << 0),	/* RW */
};

enum {
	/* SDC_ACMD19_TRG mask */
	SDC_ACMD19_TRG_TUNESEL	= (0xf  << 0),	/* RW */
};

enum {
	/* MSDC_DMA_CTRL mask */
	MSDC_DMA_CTRL_START	= (0x1  << 0),	/* W */
	MSDC_DMA_CTRL_STOP	= (0x1  << 1),	/* W */
	MSDC_DMA_CTRL_RESUME	= (0x1  << 2),	/* W */
	MSDC_DMA_CTRL_MODE	= (0x1  << 8),	/* RW */
	MSDC_DMA_CTRL_LASTBUF	= (0x1  << 10),	/* RW */
	MSDC_DMA_CTRL_BRUSTSZ	= (0x7  << 12),	/* RW */
};

enum {
	/* MSDC_DMA_CFG mask */
	MSDC_DMA_CFG_STS	= (0x1  << 0),	/* R */
	MSDC_DMA_CFG_DECSEN	= (0x1  << 1),	/* RW */
	MSDC_DMA_CFG_AHBHPROT2	= (0x2  << 8),	/* RW */
	MSDC_DMA_CFG_ACTIVEEN	= (0x2  << 12),	/* RW */
	MSDC_DMA_CFG_CS12B16B	= (0x1  << 16),	/* RW */
};

enum {
	/* MSDC_PATCH_BIT mask */
	MSDC_PATCH_BIT_ODDSUPP		= (0x1  <<  1),	/* RW */
	MSDC_INT_DAT_LATCH_CK_SEL	= (0x7  <<  7),
	MSDC_CKGEN_MSDC_DLY_SEL		= (0x1F << 10),
	MSDC_PATCH_BIT_IODSSEL		= (0x1  << 16),	/* RW */
	MSDC_PATCH_BIT_IOINTSEL		= (0x1  << 17),	/* RW */
	MSDC_PATCH_BIT_BUSYDLY		= (0xf  << 18),	/* RW */
	MSDC_PATCH_BIT_WDOD		= (0xf  << 22),	/* RW */
	MSDC_PATCH_BIT_IDRTSEL		= (0x1  << 26),	/* RW */
	MSDC_PATCH_BIT_CMDFSEL		= (0x1  << 27),	/* RW */
	MSDC_PATCH_BIT_INTDLSEL		= (0x1  << 28),	/* RW */
	MSDC_PATCH_BIT_SPCPUSH		= (0x1  << 29),	/* RW */
	MSDC_PATCH_BIT_DECRCTMO		= (0x1  << 30),	/* RW */
};

enum {
	/* MSDC_PATCH_BIT1 mask */
	MSDC_PATCH_BIT1_WRDAT_CRCS	= (0x7 << 0),
	MSDC_PATCH_BIT1_CMD_RSP		= (0x7 << 3),
	MSDC_PATCH_BIT1_GET_CRC_MARGIN	= (0x01 << 7),	/* RW */
	MSDC_PATCH_BIT1_STOP_DLY	= (0xf << 8),	/* RW */
};

enum {
	/* MSDC_PATCH_BIT2 mask */
	MSDC_PB2_SUPPORT_64G		= (0x1 << 1),	/* RW */
	MSDC_PB2_RESPWAIT		= (0x3 << 2),	/* RW */
	MSDC_PATCH_BIT2_CFGRESP		= (0x1 << 15),	/* RW */
	MSDC_PB2_RESPSTSENSEL		= (0x1 << 16),	/* RW */
	MSDC_PATCH_BIT2_CFGCRCSTS	= (0x1 << 28),	/* RW */
	MSDC_PB2_CRCSTSENSEL		= (0x1 << 29),	/* RW */
};

enum {
	/* MSDC_PAD_CTL0 mask */
	MSDC_PAD_CTL0_CLKDRVN	= (0x7  << 0),	/* RW */
	MSDC_PAD_CTL0_CLKDRVP	= (0x7  << 4),	/* RW */
	MSDC_PAD_CTL0_CLKSR	= (0x1  << 8),	/* RW */
	MSDC_PAD_CTL0_CLKPD	= (0x1  << 16),	/* RW */
	MSDC_PAD_CTL0_CLKPU	= (0x1  << 17),	/* RW */
	MSDC_PAD_CTL0_CLKSMT	= (0x1  << 18),	/* RW */
	MSDC_PAD_CTL0_CLKIES	= (0x1  << 19),	/* RW */
	MSDC_PAD_CTL0_CLKTDSEL	= (0xf  << 20),	/* RW */
	MSDC_PAD_CTL0_CLKRDSEL	= (0xffUL << 24),	/* RW */
};

enum {
	/* MSDC_PAD_CTL1 mask */
	MSDC_PAD_CTL1_CMDDRVN	= (0x7  << 0),	/* RW */
	MSDC_PAD_CTL1_CMDDRVP	= (0x7  << 4),	/* RW */
	MSDC_PAD_CTL1_CMDSR	= (0x1  << 8),	/* RW */
	MSDC_PAD_CTL1_CMDPD	= (0x1  << 16),	/* RW */
	MSDC_PAD_CTL1_CMDPU	= (0x1  << 17),	/* RW */
	MSDC_PAD_CTL1_CMDSMT	= (0x1  << 18),	/* RW */
	MSDC_PAD_CTL1_CMDIES	= (0x1  << 19),	/* RW */
	MSDC_PAD_CTL1_CMDTDSEL	= (0xf  << 20),	/* RW */
	MSDC_PAD_CTL1_CMDRDSEL	= (0xffUL << 24),	/* RW */
};

enum {
	/* MSDC_PAD_CTL2 mask */
	MSDC_PAD_CTL2_DATDRVN	= (0x7  << 0),	/* RW */
	MSDC_PAD_CTL2_DATDRVP	= (0x7  << 4),	/* RW */
	MSDC_PAD_CTL2_DATSR	= (0x1  << 8),	/* RW */
	MSDC_PAD_CTL2_DATPD	= (0x1  << 16),	/* RW */
	MSDC_PAD_CTL2_DATPU	= (0x1  << 17),	/* RW */
	MSDC_PAD_CTL2_DATIES	= (0x1  << 19),	/* RW */
	MSDC_PAD_CTL2_DATSMT	= (0x1  << 18),	/* RW */
	MSDC_PAD_CTL2_DATTDSEL	= (0xf  << 20),	/* RW */
	MSDC_PAD_CTL2_DATRDSEL	= (0xffUL << 24),	/* RW */
};

enum {
	/* MSDC_PAD_TUNE mask */
	MSDC_PAD_TUNE_DATWRDLY	= (0x1F << 0),	/* RW */
	MSDC_PAD_TUNE_DATRRDLY	= (0x1F << 8),	/* RW */
	MSDC_PAD_TUNE_RD_SEL	= (0x1 << 13),	/* RW */
	MSDC_PAD_TUNE_RXDLYSEL	= (0x1 << 15),	/* RW */
	MSDC_PAD_TUNE_CMDRDLY	= (0x1F << 16),	/* RW */
	MSDC_PAD_TUNE_CMD_SEL	= (0x1 << 21),	/* RW */
	MSDC_PAD_TUNE_CMDRRDLY	= (0x1FUL << 22),	/* RW */
	MSDC_PAD_TUNE_CLKTXDLY	= (0x1FUL << 27),	/* RW */
};

enum {
	/* MSDC_DAT_RDDLY0/1 mask */
	MSDC_DAT_RDDLY0_D3	= (0x1F << 0),	/* RW */
	MSDC_DAT_RDDLY0_D2	= (0x1F << 8),	/* RW */
	MSDC_DAT_RDDLY0_D1	= (0x1F << 16),	/* RW */
	MSDC_DAT_RDDLY0_D0	= (0x1FUL << 24),	/* RW */
};

enum {
	MSDC_DAT_RDDLY1_D7	= (0x1F << 0),	/* RW */
	MSDC_DAT_RDDLY1_D6	= (0x1F << 8),	/* RW */
	MSDC_DAT_RDDLY1_D5	= (0x1F << 16),	/* RW */
	MSDC_DAT_RDDLY1_D4	= (0x1FUL << 24),	/* RW */
};

enum {
	/* EMMC50_CFG0 mask */
	EMMC50_CFG_CFCSTS_SEL	= (0x1 << 4),	/* RW */
};

enum {
	/* SDC_FIFO_CFG mask */
	SDC_FIFO_CFG_WRVALIDSEL	= (0x1 << 24),	/* RW */
	SDC_FIFO_CFG_RDVALIDSEL	= (0x1 << 25),	/* RW */
};

enum {
	/* EMMC_TOP_CONTROL mask */
	PAD_RXDLY_SEL		= (0x1 << 0),	/* RW */
	DELAY_EN		= (0x1 << 1),	/* RW */
	PAD_DAT_RD_RXDLY2	= (0x1f << 2),	/* RW */
	PAD_DAT_RD_RXDLY	= (0x1f << 7),	/* RW */
	PAD_DAT_RD_RXDLY2_SEL	= (0x1 << 12),	/* RW */
	PAD_DAT_RD_RXDLY_SEL	= (0x1 << 13),	/* RW */
	DATA_K_VALUE_SEL	= (0x1 << 14),	/* RW */
	SDC_RX_ENH_EN		= (0x1 << 15),	/* TW */
};

enum {
	/* EMMC_TOP_CMD mask */
	PAD_CMD_RXDLY2		= (0x1f << 0),	/* RW */
	PAD_CMD_RXDLY		= (0x1f << 5),	/* RW */
	PAD_CMD_RD_RXDLY2_SEL	= (0x1 << 10),	/* RW */
	PAD_CMD_RD_RXDLY_SEL	= (0x1 << 11),	/* RW */
	PAD_CMD_TX_DLY		= (0x1f << 12),	/* RW */
};

enum {
	MSDC_BUS_1BITS		= 0,
	MSDC_BUS_4BITS		= 1,
	MSDC_BUS_8BITS		= 2
};

enum {
	DEFAULT_DTOC		= 3
};

enum {
	MSDC_MS			= 0,
	MSDC_SDMMC		= 1
};

enum {
	MTK_MMC_TIMEOUT_MS = 1000,
};

#endif // __DRIVERS_STORAGE_MTK_MMC_PRIVATE_H_

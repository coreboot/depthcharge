/*
  * Copyright (c) 2015, Intel Corporation.
  * Copyright (c) 2015, Google Inc.
  *
  * See file CREDITS for list of people who contributed to this
  * project.
  *
  * This program is free software; you can redistribute it and/or modify it
  * under the terms and conditions of the GNU General Public License,
  * version 2, as published by the Free Software Foundation.
  *
  * This program is distributed in the hope it will be useful, but WITHOUT
  * ANY WARRANTY; without evenp the implied warranty of MERCHANTABILITY or
  * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  * more details.
  *
  * You should have received a copy of the GNU General Public License along with
  * this program; if not, write to the Free Software Foundation, Inc.,
  * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  */

#ifndef __DRIVERS_BUS_I2S_BRASWELL_BRASWELL_REGS_H__
#define __DRIVERS_BUS_I2S_BRASWELL_BRASWELL_REGS_H__

#include <libpayload.h>

/* FIFO over/under-run interrupt config. */
enum {
	SSP_FIFO_INT_ENABLE = 0x0,
	SSP_FIFO_INT_DISABLE,
};

/* Frame format. */
enum {
	MOTOROLA_SPI_FORMAT = 0x0,
	TI_SSP_FORMAT,
	MICROWIRE_FORMAT,
	PSP_FORMAT,
};

enum {
	PCI_ADID_SSP_MODE_SPI = 1,
	PCI_ADID_SSP_MODE_I2S = 2,
};

enum {
	PCI_ADID_SSP_CONF_BT_FM = 1 << 2,
	PCI_ADID_SSP_CONF_MODEM = 2 << 2 ,
};

enum {
	PCI_ADID_SSP_FS_GPIO_MAPPING_SHIFT = 6,
	PCI_ADID_SSP_FS_GPIO_MAPPING_MASK = 0xFF,
};

enum {
	PCI_ADID_SSP_FS_GPIO_MODE_SHIFT = 14,
	PCI_ADID_SSP_FS_GPIO_MODE_MASK = 0x3,
};

enum {
	PCI_CAP_ADID_I2S_BT_FM =
		PCI_ADID_SSP_CONF_BT_FM | PCI_ADID_SSP_MODE_I2S,
	PCI_CAP_ADID_I2S_MODEM =
		PCI_ADID_SSP_CONF_MODEM | PCI_ADID_SSP_MODE_I2S,
};

enum {
	SSP_PLL_FREQ_05_622 = 0 << 4,
	SSP_PLL_FREQ_11_345 = 1 << 4,
	SSP_PLL_FREQ_12_235 = 2 << 4,
	SSP_PLL_FREQ_14_847 = 3 << 4,
	SSP_PLL_FREQ_32_842 = 4 << 4,
	SSP_PLL_FREQ_48_000 = 5 << 4,
};

enum {
	SSP_SYSCLK_DIV4_BYPASS = 1 << 3,

	SSP_SYSCLK_DIV_1 = 0 << 0,
	SSP_SYSCLK_DIV_2 = 1 << 0,
	SSP_SYSCLK_DIV_4 = 2 << 0,
	SSP_SYSCLK_DIV_8 = 3 << 0,
	SSP_SYSCLK_DIV_16 = 4 << 0,
	SSP_SYSCLK_DIV_32 = 5 << 0,
};

enum {
	SSP_CLK_SSCR0_SCR_NOT_AVAILABLE = 0,
	SSP_SSACD_NOT_AVAILABLE = 0xFF,
};

enum {
	BSW_SSP2_START_ADDRESS = 0X0A2000,
	BSW_SSP2_SHIM_START_ADDRESS = 0x140000,
};

enum {
	SSP_IN_MASTER_MODE = 0x0,
	SSP_IN_SLAVE_MODE = 0x1,
};

enum {
	SSP_DATA_TX = 0,
	SSP_DATA_RX = 1,
};


#define DEFINE_REG(reg, offset) \
	const uint8_t OFFSET_ ## reg = offset; \
	static inline uint32_t read_ ## reg(void *p) \
		{ return readl(p + (OFFSET_ ## reg)); } \
	static inline void write_ ## reg(uint32_t v, void *p) \
		{ writel(v, p + (OFFSET_ ## reg)); }

#define DEFINE_FIELD(reg, field, mask, shift) \
	const uint32_t reg ## _ ## field ## _MASK = (uint32_t)(mask);  \
	const uint8_t  reg ## _ ## field ## _SHIFT = (uint8_t)(shift);  \
							\
	static inline uint32_t extract_ ## reg ## _ ## field \
				(uint32_t reg_value) { \
		return  ((reg_value) >> reg ## _ ## field ## _SHIFT) \
				& reg ## _ ## field ## _MASK; \
	} \
	  \
	static inline uint32_t replace_ ## reg ## _ ## field \
				(uint32_t reg_value, uint32_t field_value) { \
		return  (((field_value) & reg ## _ ## field ## _MASK) \
			<< reg ## _ ## field ## _SHIFT) \
			| ((reg_value) & ~(reg ## _ ## field ## _MASK \
			<< reg ## _ ## field ## _SHIFT)); \
	} \
	  \
	static inline uint32_t set_ ## reg ## _ ## field \
				(uint32_t reg_value) { \
		return  ((reg ## _ ## field ## _MASK) \
			<< reg ## _ ## field ## _SHIFT) \
			| ((reg_value) & ~(reg ## _ ## field ## _MASK \
			<< reg ## _ ## field ## _SHIFT)); \
	}

/* Register definitions. */
DEFINE_REG(LPE_CSR, 0x00)
DEFINE_REG(LPE_PISR, 0x08)
DEFINE_REG(LPE_PIMR, 0x10)
DEFINE_REG(LPE_ISRX, 0x18)
DEFINE_REG(LPE_IMRX, 0x28)
DEFINE_REG(LPE_IPCX, 0x38)
DEFINE_REG(LPE_IPCD, 0x40)
DEFINE_REG(LPE_ISRD, 0x20)
DEFINE_REG(LPE_CLKCTL, 0x78)
DEFINE_REG(LPE_SSP0_DIVC_L, 0xE8)
DEFINE_REG(LPE_SSP0_DIVC_H, 0xEC)
DEFINE_REG(LPE_SSP1_DIVC_L, 0xF0)
DEFINE_REG(LPE_SSP1_DIVC_H, 0xF4)
DEFINE_REG(LPE_SSP2_DIVC_L, 0xF8)
DEFINE_REG(LPE_SSP2_DIVC_H, 0xFC)

/* LPE_ISRX fields definitions */
DEFINE_FIELD(LPE_ISRX, IAPIS_SSP0, 0x01, 3)
DEFINE_FIELD(LPE_ISRX, IAPIS_SSP1, 0x01, 4)
DEFINE_FIELD(LPE_ISRX, IAPIS_SSP2, 0x01, 5)

/* SSP registers definitions. */
DEFINE_REG(SSCR0, 0x00)
DEFINE_REG(SSCR1, 0x04)
DEFINE_REG(SSSR, 0x08)
DEFINE_REG(SSITR, 0x0C)
DEFINE_REG(SSDR, 0x10)
DEFINE_REG(SSTO, 0x28)
DEFINE_REG(SSPSP, 0x2C)
DEFINE_REG(SSTSA, 0x30)
DEFINE_REG(SSRSA, 0x34)
DEFINE_REG(SSTSS, 0x38)
DEFINE_REG(SSACD, 0x3C)
DEFINE_REG(SSCR2, 0x40)
DEFINE_REG(SSCR3, 0x70)
DEFINE_REG(SSCR4, 0x74)
DEFINE_REG(SSCR5, 0x78)
DEFINE_REG(SSFS, 0x44)
DEFINE_REG(SFIFOL, 0x68)
DEFINE_REG(SFIFOTT, 0x6C)

/* SSP SSCR0 field definitions. */
DEFINE_FIELD(SSCR0, DSS, 0x0F, 0)	/* Data Size Select [4..16] */
DEFINE_FIELD(SSCR0, FRF, 0x03, 4)	/* Frame Format */
DEFINE_FIELD(SSCR0, ECS, 0x01, 6)	/* External clock select */
DEFINE_FIELD(SSCR0, SSE, 0x01, 7)	/* Synchronous Serial Port Enable */
DEFINE_FIELD(SSCR0, SCR, 0xFFF, 8)	/* Not implemented */
DEFINE_FIELD(SSCR0, EDSS, 0x1, 20)	/* Extended data size select */
DEFINE_FIELD(SSCR0, NCS, 0x1, 21)	/* Network clock select */
DEFINE_FIELD(SSCR0, RIM, 0x1, 22)	/* Receive FIFO overrrun int mask */
DEFINE_FIELD(SSCR0, TIM, 0x1, 23)	/* Transmit FIFO underrun int mask */
DEFINE_FIELD(SSCR0, FRDC, 0x7, 24)	/* Frame Rate Divider Control */
DEFINE_FIELD(SSCR0, ACS, 0x1, 30)	/* Audio clock select */
DEFINE_FIELD(SSCR0, MOD, 0x1, 31)	/* Mode (normal or network) */

#define SSCR0_DataSize(x)     ((x) - 1)	/* Data Size Select [4..16] */
#define SSCR0_SlotsPerFrm(x)  ((x) - 1)	/* Time slots per frame */
#define SSCR0_SerClkDiv(x)    ((x) - 1)	/* Divisor [1..4096],... */

/* SSP SSCR1 fields definitions */
DEFINE_FIELD(SSCR1, TTELP, 0x1, 31)	/* TXD Tristate Enable on Last Phase */
DEFINE_FIELD(SSCR1, TTE, 0x1, 30)	/* TXD Tristate Enable */
DEFINE_FIELD(SSCR1, EBCEI, 0x1, 29)	/* Enable Bit Count Error Interrupt */
DEFINE_FIELD(SSCR1, SCFR, 0x1, 28)	/* Slave Clock Running */
DEFINE_FIELD(SSCR1, ECRA, 0x1, 27)	/* Enable Clock Request A */
DEFINE_FIELD(SSCR1, ECRB, 0x1, 26)	/* Enable Clock Request B */
DEFINE_FIELD(SSCR1, SCLKDIR, 0x1, 25)	/* SSPCLK Direction */
DEFINE_FIELD(SSCR1, SFRMDIR, 0x1, 24)	/* SSPFRM Direction */
DEFINE_FIELD(SSCR1, RWOT, 0x1, 23)	/* Receive without Transmit */
DEFINE_FIELD(SSCR1, TRAIL, 0x1, 22)	/* Trailing Byte */
DEFINE_FIELD(SSCR1, TSRE, 0x1, 21)	/* DMA Tx Service Request Enable */
DEFINE_FIELD(SSCR1, RSRE, 0x1, 20)	/* DMA Rx Service Request Enable */
DEFINE_FIELD(SSCR1, TINTE, 0x1, 19)	/* Receiver Timeout Interrupt Enable */
DEFINE_FIELD(SSCR1, PINTE, 0x1, 18)	/* Periph. Trailing Byte Int. Enable */
DEFINE_FIELD(SSCR1, IFS, 0x1, 16)	/* Invert Frame Signal */
DEFINE_FIELD(SSCR1, STRF, 0x1, 15)	/* Select FIFO for EFWR: test mode */
DEFINE_FIELD(SSCR1, EFWR, 0x1, 14)	/* Enable FIFO Write/Read: test mode */
DEFINE_FIELD(SSCR1, RFT, 0xF, 10)	/* Receive FIFO Trigger Threshold */
DEFINE_FIELD(SSCR1, TFT, 0xF, 6)	/* Transmit FIFO Trigger Threshold */
DEFINE_FIELD(SSCR1, MWDS, 0x1, 5)	/* Microwire Transmit Data Size */
DEFINE_FIELD(SSCR1, SPH, 0x1, 4)	/* Motorola SPI SSPSCLK phase setting*/
DEFINE_FIELD(SSCR1, SPO, 0x1, 3)	/* Motorola SPI SSPSCLK polarity */
DEFINE_FIELD(SSCR1, LBM, 0x1, 2)	/* Loopback mode: test mode */
DEFINE_FIELD(SSCR1, TIE, 0x1, 1)	/* Transmit FIFO Interrupt Enable */
DEFINE_FIELD(SSCR1, RIE, 0x1, 0)	/* Receive FIFO Interrupt Enable */

#define SSCR1_RxTresh(x) ((x) - 1)	/* level [1..16] */
#define SSCR1_TxTresh(x) (x)		/* level [1..16] */

DEFINE_FIELD(SSCR2, ACG_EN, 0x1, 4)
DEFINE_FIELD(SSCR2, CLK_DEL_EN, 0x1, 3)
DEFINE_FIELD(SSCR2, SLV_EXT_CLK_RUN_EN, 0x1, 2)
DEFINE_FIELD(SSCR2, UNDERRUN_FIX_1, 0x1, 1)
DEFINE_FIELD(SSCR2, UNDERRUN_FIX_0, 0x1, 1)
DEFINE_FIELD(SSCR3, MST_CLK_EN, 0x1, 16)
DEFINE_FIELD(SSCR3, STRETCH_RX, 0x1, 15)
DEFINE_FIELD(SSCR3, STRETCH_TX, 0x1, 14)
DEFINE_FIELD(SSCR3, I2S_RX_EN, 0x1, 10)
DEFINE_FIELD(SSCR3, I2S_TX_EN, 0x1, 9)
DEFINE_FIELD(SSCR3, I2S_RX_SS_FIX_EN, 0x1, 4)
DEFINE_FIELD(SSCR3, I2S_TX_SS_FIX_EN, 0x1, 3)
DEFINE_FIELD(SSCR3, I2S_MODE_EN, 0x1, 1)
DEFINE_FIELD(SSCR3, FRM_MST_EN, 0x1, 0)

DEFINE_FIELD(SSPSP, FSRT, 0x1, 25)
DEFINE_FIELD(SSPSP, DMYSTOP, 0x3, 23)
DEFINE_FIELD(SSPSP, SFRMWDTH, 0x3F, 16)
DEFINE_FIELD(SSPSP, SFRMDLY, 0x7F, 9)
DEFINE_FIELD(SSPSP, DMYSTRT, 0x3, 7)
DEFINE_FIELD(SSPSP, STRTDLY, 0x7, 4)
DEFINE_FIELD(SSPSP, ETDS, 0x1, 3)
DEFINE_FIELD(SSPSP, SFRMP, 0x1, 2)
DEFINE_FIELD(SSPSP, SCMODE, 0x3, 0)

DEFINE_FIELD(SSTSA, TTSA, 0xFF, 0)
DEFINE_FIELD(SSRSA, RTSA, 0xFF, 0)

DEFINE_FIELD(SSSR, BCE, 0x1, 23)	/* Bit Count Error: RW 1 to Clear */
DEFINE_FIELD(SSSR, CSS, 0x1, 22)	/* Clock Synchronization Status */
DEFINE_FIELD(SSSR, TUR, 0x1, 21)	/* Tx FIFO UnderRun: RW 1 to Clear */
DEFINE_FIELD(SSSR, EOC, 0x1, 20)	/* End Of Chain: RW 1 to Clear */
DEFINE_FIELD(SSSR, TINT, 0x1, 19)	/* Receiver Time-out Interrupt: */
DEFINE_FIELD(SSSR, PINT, 0x1, 18)	/* Periph. Trailing Byte Interrupt: */
DEFINE_FIELD(SSSR, RFL, 0xF, 12)	/* Receive FIFO Level */
DEFINE_FIELD(SSSR, TFL, 0xF, 8)		/* Transmit FIFO Level */
DEFINE_FIELD(SSSR, ROR, 0x1, 7)		/* Rx FIFO Overrun: RW 1 to Clear */
DEFINE_FIELD(SSSR, RFS, 0x1, 6)		/* Receive FIFO Service Request */
DEFINE_FIELD(SSSR, TFS, 0x1, 5)		/* Transmit FIFO Service Request */
DEFINE_FIELD(SSSR, BSY, 0x1, 4)		/* SSP Busy */
DEFINE_FIELD(SSSR, RNE, 0x1, 3)		/* Receive FIFO not empty */
DEFINE_FIELD(SSSR, TNF, 0x1, 2)		/* Transmit FIFO not Full */

DEFINE_FIELD(SSCR4, TOT_FRM_PRD, 0x1FF, 7)
DEFINE_FIELD(SSCR5, FRM_ASRT_WIDTH, 0x1FFFFFF, 1)

DEFINE_FIELD(LPE_SSP0_DIVC_H, DIV_BYPASS, 0x1, 31)
DEFINE_FIELD(LPE_SSP0_DIVC_H, DIV_EN, 0x1, 30)
DEFINE_FIELD(LPE_SSP0_DIVC_H, DIV_UPDATE, 0x1, 29)
DEFINE_FIELD(LPE_SSP0_DIVC_H, DIV_M, 0xFFFFF, 0)
DEFINE_FIELD(LPE_SSP0_DIVC_L, DIV_N, 0xFFFFF, 0)

#define SSCR0_reg(regbit, value)					\
	(((value) & SSCR0_##regbit##_MASK) << SSCR0_##regbit##_SHIFT)

#define SSCR1_reg(regbit, value)					\
	(((value) & SSCR1_##regbit##_MASK) << SSCR1_##regbit##_SHIFT)

#define SSPSP_reg(regbit, value)					\
	(((value) & SSPSP_##regbit##_MASK) << SSPSP_##regbit##_SHIFT)

#define SSRSA_reg(regbit, value)					\
	(((value) & SSRSA_##regbit##_MASK) << SSRSA_##regbit##_SHIFT)
#define SSTSA_reg(regbit, value)					\
	(((value) & SSTSA_##regbit##_MASK) << SSTSA_##regbit##_SHIFT)

#define SSCR4_reg(regbit, value)					\
	(((value) & SSCR4_##regbit##_MASK) << SSCR4_##regbit##_SHIFT)

#define SSCR5_reg(regbit, value)					\
	(((value) & SSCR5_##regbit##_MASK) << SSCR5_##regbit##_SHIFT)

#define LPE_SSP0_DIVC_H_reg(regbit, value)				\
	(((value) & LPE_SSP0_DIVC_H_##regbit##_MASK) << \
		LPE_SSP0_DIVC_H_##regbit##_SHIFT)

#define LPE_SSP0_DIVC_L_reg(regbit, value)				\
	(((value) & LPE_SSP0_DIVC_L_##regbit##_MASK) << \
		LPE_SSP0_DIVC_L_##regbit##_SHIFT)

#define set_SSCR0_reg(reg_pointer, regbit)				  \
	write_SSCR0(read_SSCR0(reg_pointer)				  \
	| (SSCR0_##regbit##_MASK << SSCR0_##regbit##_SHIFT),		\
	reg_pointer);

#define clear_SSCR0_reg(reg_pointer, regbit)				  \
	write_SSCR0((read_SSCR0(reg_pointer)				  \
	& (~((SSCR0_##regbit##_MASK << SSCR0_##regbit##_SHIFT)))),	  \
	reg_pointer);

#define set_SSCR3_reg(reg_pointer, regbit)				  \
	write_SSCR3(read_SSCR3(reg_pointer)				  \
	| (SSCR3_##regbit##_MASK << SSCR3_##regbit##_SHIFT),		\
	reg_pointer);

#define clear_SSCR3_reg(reg_pointer, regbit)				  \
	write_SSCR3((read_SSCR3(reg_pointer)				  \
	& (~((SSCR3_##regbit##_MASK << SSCR3_##regbit##_SHIFT)))),	  \
	reg_pointer);

#define GET_SSSR_val(x, regb)						  \
	((x & (SSSR_##regb##_MASK<<SSSR_##regb##_SHIFT))>>SSSR_##regb##_SHIFT)

typedef struct BswI2sRegs {
	uint32_t sscr0;
	uint32_t sscr1;
	uint32_t sssr;
	uint32_t ssitr;
	uint32_t ssdr;
	uint32_t ssto;
	uint32_t sspsp;
	uint32_t sstsa;
	uint32_t ssrsa;
	uint32_t sstss;
	uint32_t ssacd;
	uint32_t sscr2;
	uint32_t ssfs;
	uint32_t frame_cnt0;
	uint32_t frame_cnt1;
	uint32_t frame_cnt2;
	uint32_t frame_cnt3;
	uint32_t frame_cnt4;
	uint32_t frame_cnt5;
	uint32_t frame_cnt6;
	uint32_t frame_cnt7;
	uint32_t sfifol;
	uint32_t sfifott;
	uint32_t sscr3;
	uint32_t sscr4;
	uint32_t sscr5;
	uint32_t asrc_frt;
	uint32_t asrc_ftc;
	uint32_t asrc_snpsht;
	uint32_t asrc_frmcnt;
} __attribute__ ((packed)) BswI2sRegs;

#endif

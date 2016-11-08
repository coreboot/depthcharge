/*
  * Copyright (c) 2016, Intel Corporation.
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
  */

#ifndef __DRIVERS_BUS_I2S_APOLLOLAKE_APOLLOLAKE_REGS_H__
#define __DRIVERS_BUS_I2S_APOLLOLAKE_APOLLOLAKE_REGS_H__

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
	APL_SSP5_START_ADDRESS = 0x3000,
	APL_SSP5_SHIM_START_ADDRESS = 0x800,
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

/* SSP registers definitions. */
DEFINE_REG(SSCR0, 0x00)
DEFINE_REG(SSCR1, 0x04)
DEFINE_REG(SSSR, 0x08)
DEFINE_REG(SSTO, 0x28)
DEFINE_REG(SSPSP, 0x2C)
DEFINE_REG(SSTSA, 0x30)
DEFINE_REG(SSRSA, 0x34)
DEFINE_REG(SSCR2, 0x40)
DEFINE_REG(SSPSP2, 0x44)
DEFINE_REG(SSCR3, 0x48)
DEFINE_REG(SSIOC, 0x4C)

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
DEFINE_FIELD(SSCR0, MOD, 0x1, 31)	/* Mode (normal or network) */

#define SSCR0_DataSize(x)     ((x) - 1)	/* Data Size Select [4..16] */
#define SSCR0_SlotsPerFrm(x)  ((x) - 1)	/* Time slots per frame */

/* SSP SSCR1 fields definitions */
DEFINE_FIELD(SSCR1, TTELP, 0x1, 31)	/* TXD Tristate Enable on Last Phase */
DEFINE_FIELD(SSCR1, TTE, 0x1, 30)	/* TXD Tristate Enable */
DEFINE_FIELD(SSCR1, TRAIL, 0x1, 22)	/* Trailing Byte */
DEFINE_FIELD(SSCR1, TSRE, 0x1, 21)	/* DMA Tx Service Request Enable */
DEFINE_FIELD(SSCR1, RSRE, 0x1, 20)	/* DMA Rx Service Request Enable */

/* SSP SSCR2 fields definitions */
DEFINE_FIELD(SSCR2, SDFD, 0x1, 14)
DEFINE_FIELD(SSCR2, TURM1, 0x1, 1)

/* SSP SSIOC fields definitions */
DEFINE_FIELD(SSIOC, SFCR, 0x1, 4)	/* SFCR SSP Force Clock Running  */
DEFINE_FIELD(SSIOC, SCOE, 0x1, 5)	/* SCOE SSP Clock Output Enable */

/* SSP SSPSP fields definitions */
DEFINE_FIELD(SSPSP, FSRT, 0x1, 25)
DEFINE_FIELD(SSPSP, EDMYSTOP, 0x7, 26)
DEFINE_FIELD(SSPSP, SFRMWDTH, 0x3F, 16)

/* SSP SSTSA fields definitions */
DEFINE_FIELD(SSTSA, TXEN, 0x1, 8)
DEFINE_FIELD(SSTSA, TTSA, 0xFF, 0)

/* SSP SSRSA fields definitions */
DEFINE_FIELD(SSRSA, RTSA, 0xFF, 0)

#define SSCR0_reg(regbit, value)					\
	(((value) & SSCR0_##regbit##_MASK) << SSCR0_##regbit##_SHIFT)

#define SSCR1_reg(regbit, value)					\
	(((value) & SSCR1_##regbit##_MASK) << SSCR1_##regbit##_SHIFT)

#define SSCR2_reg(regbit, value)					\
	(((value) & SSCR2_##regbit##_MASK) << SSCR2_##regbit##_SHIFT)

#define SSPSP_reg(regbit, value)					\
	(((value) & SSPSP_##regbit##_MASK) << SSPSP_##regbit##_SHIFT)

#define SSRSA_reg(regbit, value)					\
	(((value) & SSRSA_##regbit##_MASK) << SSRSA_##regbit##_SHIFT)

#define SSIOC_reg(regbit, value)					\
	(((value) & SSIOC_##regbit##_MASK) << SSIOC_##regbit##_SHIFT)

#define SSTSA_reg(regbit, value)					\
	(((value) & SSTSA_##regbit##_MASK) << SSTSA_##regbit##_SHIFT)

#define set_SSCR0_reg(reg_pointer, regbit)				  \
	write_SSCR0(read_SSCR0(reg_pointer)				  \
	| (SSCR0_##regbit##_MASK << SSCR0_##regbit##_SHIFT),		\
	reg_pointer);

#define clear_SSCR0_reg(reg_pointer, regbit)				  \
	write_SSCR0((read_SSCR0(reg_pointer)				  \
	& (~((SSCR0_##regbit##_MASK << SSCR0_##regbit##_SHIFT)))),	  \
	reg_pointer);

#define set_SSTSA_reg(reg_pointer, regbit)                                \
	write_SSTSA(read_SSTSA(reg_pointer)                               \
	| (SSTSA_##regbit##_MASK << SSTSA_##regbit##_SHIFT),              \
	reg_pointer);

#define clear_SSTSA_reg(reg_pointer, regbit)				  \
	write_SSTSA((read_SSTSA(reg_pointer)				  \
	& (~((SSTSA_##regbit##_MASK << SSTSA_##regbit##_SHIFT)))),	  \
	reg_pointer);

#define set_SSCR3_reg(reg_pointer, regbit)				  \
	write_SSCR3(read_SSCR3(reg_pointer)				  \
	| (SSCR3_##regbit##_MASK << SSCR3_##regbit##_SHIFT),		\
	reg_pointer);

#define clear_SSCR3_reg(reg_pointer, regbit)				  \
	write_SSCR3((read_SSCR3(reg_pointer)				  \
	& (~((SSCR3_##regbit##_MASK << SSCR3_##regbit##_SHIFT)))),	  \
	reg_pointer);

typedef struct AplI2sRegs {
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
} __attribute__ ((packed)) AplI2sRegs;

#endif

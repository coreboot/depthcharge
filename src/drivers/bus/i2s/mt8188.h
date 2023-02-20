/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2023 Mediatek Inc.
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

#ifndef __DRIVERS_BUS_I2S_MT8188_H__
#define __DRIVERS_BUS_I2S_MT8188_H__

#define SRAM_BASE 0x10b20000
#define SRAM_SIZE 0x10000

/* AFE */
typedef struct afe_reg {
	uint32_t top_con0;		/* 0x0000: AUDIO_TOP_CON0 */
	uint32_t top_con1;		/* 0x0004: AUDIO_TOP_CON1 */
	uint32_t _rsv0[2];
	uint32_t top_con4;		/* 0x0010: AUDIO_TOP_CON4 */
	uint32_t top_con5;		/* 0x0014: AUDIO_TOP_CON5 */
	uint32_t _rsv1[378];
	uint32_t asys_top_con;		/* 0x0600: ASYS_TOP_CON */
	uint32_t _rsv2[767];
	uint32_t dac_con0;		/* 0x1200: AFE_DAC_CON0 */
	uint32_t _rsv3[19];
	uint32_t dl2_base;		/* 0x1250: AFE_DL2_BASE */
	uint32_t dl2_cur;		/* 0x1254: AFE_DL2_CUR */
	uint32_t dl2_end;		/* 0x1258: AFE_DL2_END */
	uint32_t dl2_con0;		/* 0x125C: AFE_DL2_CON0 */
	uint32_t _rsv4[208];
	uint32_t memif_agent_fs_con0;	/* 0x15a0: AFE_MEMIF_AGENT_FS_CON0 */
	uint32_t _rsv5[828];
	uint32_t in_afifo_con[2];	/* 0x2294: ETDM_IN1_AFIFO_CON */
	uint32_t _rsv6[21];
	uint32_t etdm_cowork_con0;	/* 0x22f0: ETDM_COWORK_CON0 */
	uint32_t etdm_cowork_con1;	/* 0x22f4: ETDM_COWORK_CON1 */
	uint32_t etdm_cowork_con2;	/* 0x22f8: ETDM_COWORK_CON2 */
	uint32_t etdm_cowork_con3;	/* 0x22fC: ETDM_COWORK_CON3 */
	struct {
		uint32_t con[6];
		uint32_t reserved[2];
	} etdm_in[2];			/* 0x2300: ETDM_IN1_CON0 */
	uint32_t _rsv7[16];
	struct {
		uint32_t con[6];
		uint32_t reserved[2];
	} etdm_out[2];			/* 0x2380: ETDM_OUT1_CON0 */
	uint32_t _rsv8[1146];
	uint32_t conn72_2;		/* 0x35a8: AFE_CONN72_2 */
	uint32_t _rsv9[4];
	uint32_t conn73_2;		/* 0x35bc: AFE_CONN73_2 */
} __attribute__((packed)) MtkI2sRegs;

check_member(afe_reg, asys_top_con, 0x0600);
check_member(afe_reg, dac_con0, 0x1200);
check_member(afe_reg, dl2_base, 0x1250);
check_member(afe_reg, in_afifo_con[0], 0x2294);
check_member(afe_reg, etdm_cowork_con0, 0x22f0);
check_member(afe_reg, etdm_out[0].con[0], 0x2380);
check_member(afe_reg, conn72_2, 0x35a8);
check_member(afe_reg, conn73_2, 0x35bc);

#define MTK_MEMIF_CHANNEL(r)	r->dl2_con0
#define MTK_MEMIF_RATE(r)	r->memif_agent_fs_con0
#define MTK_MEMIF_BITWIDTH(r)	r->dl2_con0

/* DL2 I70/I71 -> ETDM_OUT1 O72/O73 connection */
#define MTK_I2SO1_LCH(r)	r->conn72_2
#define MTK_I2SO1_RCH(r)	r->conn73_2
#define MTK_I2SO2_LCH(r)	r->conn72_2 /* O1 duplicate */
#define MTK_I2SO2_RCH(r)	r->conn73_2 /* O1 duplicate */

#define MTK_LCH_DATA	BIT(6)
#define MTK_RCH_DATA	BIT(7)

enum {
	MTK_MEMIF_CHANNEL_SFT = 0,
	MTK_MEMIF_RATE_SFT = 10,
	MTK_MEMIF_BIT_SFT = 5,
	MTK_MEMIF_ENABLE_SFT = 18,
};

/* ETDM_IN_AFIFO_CON */
#define ETDM_IN_USE_AFIFO		BIT(8)
#define ETDM_IN_AFIFO_CLOCK(x)		((x) << 5)
#define ETDM_IN_AFIFO_CLOCK_MASK	(0x7 << 5)
#define ETDM_IN_AFIFO_MODE(x)		((x) << 0)
#define ETDM_IN_AFIFO_MODE_MASK		0x1f

/* ETDM_COWORK_CON0 */
#define ETDM_OUT1_SLAVE_SEL(x)		((x) << 20)
#define ETDM_OUT1_SLAVE_SEL_MASK	(0xf << 20)
#define ETDM_OUT1_SLAVE_SEL_SHIFT	20

/* ETDM_COWORK_CON2 */
#define ETDM_IN2_SLAVE_SEL(x)		((x) << 24)
#define ETDM_IN2_SLAVE_SEL_MASK		(0xf << 24)
#define ETDM_IN2_SLAVE_SEL_SHIFT	24
#define ETDM_OUT3_SLAVE_SEL(x)		((x) << 20)
#define ETDM_OUT3_SLAVE_SEL_MASK	(0xf << 20)
#define ETDM_OUT3_SLAVE_SEL_SHIFT	20
#define ETDM_OUT2_SLAVE_SEL(x)		((x) << 8)
#define ETDM_OUT2_SLAVE_SEL_MASK	(0xf << 8)
#define ETDM_OUT2_SLAVE_SEL_SHIFT	8

/* ETDM_COWORK_CON2 */
#define ETDM_IN2_SLAVE_SEL(x)		((x) << 24)
#define ETDM_IN2_SLAVE_SEL_MASK		(0xf << 24)
#define ETDM_IN2_SLAVE_SEL_SHIFT	24
#define ETDM_OUT3_SLAVE_SEL(x)		((x) << 20)
#define ETDM_OUT3_SLAVE_SEL_MASK	(0xf << 20)
#define ETDM_OUT3_SLAVE_SEL_SHIFT	20
#define ETDM_OUT2_SLAVE_SEL(x)		((x) << 8)
#define ETDM_OUT2_SLAVE_SEL_MASK	(0xf << 8)
#define ETDM_OUT2_SLAVE_SEL_SHIFT	8

/* ETDM_COWORK_CON2 */
#define ETDM_IN2_SLAVE_SEL(x)		((x) << 24)
#define ETDM_IN2_SLAVE_SEL_MASK		(0xf << 24)
#define ETDM_IN2_SLAVE_SEL_SHIFT	24
#define ETDM_OUT3_SLAVE_SEL(x)		((x) << 20)
#define ETDM_OUT3_SLAVE_SEL_MASK	(0xf << 20)
#define ETDM_OUT3_SLAVE_SEL_SHIFT	20
#define ETDM_OUT2_SLAVE_SEL(x)		((x) << 8)
#define ETDM_OUT2_SLAVE_SEL_MASK	(0xf << 8)
#define ETDM_OUT2_SLAVE_SEL_SHIFT	8

#define ETDM_CON0_CH_NUM(x)		(((x) - 1) << 23)
#define ETDM_CON0_CH_NUM_MASK		(0x1f << 23)
#define ETDM_CON0_WORD_LEN(x)		(((x) - 1) << 16)
#define ETDM_CON0_WORD_LEN_MASK		(0x1f << 16)
#define ETDM_CON0_BIT_LEN(x)		(((x) - 1) << 11)
#define ETDM_CON0_BIT_LEN_MASK		(0x1f << 11)
#define ETDM_CON0_FORMAT(x)		((x) << 6)
#define ETDM_CON0_FORMAT_MASK		(0x7 << 6)
#define ETDM_CON0_SLAVE_MODE		BIT(5)
#define ETDM_CON0_EN			BIT(0)

#define ETDM_OUT_CON0_RELATCH_DOMAIN(x)		((x) << 28)
#define ETDM_OUT_CON0_RELATCH_DOMAIN_MASK	(0x3 << 28)

#define ETDM_IN_CON2_CLOCK(x)			((x) << 10)
#define ETDM_IN_CON2_CLOCK_MASK			(0x7 << 10)
#define ETDM_IN_CON2_CLOCK_SHIFT		10

#define ETDM_OUT_CON2_LRCK_DELAY_BCK_INV	BIT(30)
#define ETDM_OUT_CON2_LRCK_DELAY_0P5T_EN	BIT(29)

#define ETDM_IN_CON3_FS(x)			((x) << 26)
#define ETDM_IN_CON3_FS_MASK			(0x1f << 26)

#define ETDM_OUT_CON4_RELATCH_EN(x)		((x) << 24)
#define ETDM_OUT_CON4_RELATCH_EN_MASK		(0x1f << 24)
#define ETDM_OUT_CON4_CLOCK(x)			((x) << 6)
#define ETDM_OUT_CON4_CLOCK_MASK		(0x7 << 6)
#define ETDM_OUT_CON4_CLOCK_SHIFT		6
#define ETDM_OUT_CON4_FS(x)			((x) << 0)
#define ETDM_OUT_CON4_FS_MASK			0x1f

#endif /* __DRIVERS_BUS_I2S_MT8188_H__ */

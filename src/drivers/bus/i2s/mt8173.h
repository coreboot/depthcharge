/*
 * Copyright 2015 Mediatek Inc.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __DRIVERS_BUS_I2S_MTK_H__
#define __DRIVERS_BUS_I2S_MTK_H__

#include "drivers/bus/i2s/i2s.h"
#include "drivers/sound/route.h"

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/
/* AFE */
typedef struct {
	uint32_t top_con0;	/* 0x0000: AUDIO_TOP_CON0 */
	uint32_t _rsv0[3];
	uint32_t dac_con0;	/* 0x0010: AFE_DAC_CON0 */
	uint32_t dac_con1;	/* 0x0014: AFE_DAC_CON1 */
	uint32_t _rsv1[3];
	uint32_t conn1;		/* 0x0024: AFE_CONN1 */
	uint32_t conn2;		/* 0x0028: AFE_CONN2 */
	uint32_t _rsv2[2];
	uint32_t i2s_con1;	/* 0x0034: AFE_I2S_CON1 */
	uint32_t _rsv3[2];
	uint32_t dl1_base;	/* 0x0040: AFE_DL1_BASE */
	uint32_t dl1_cur;	/* 0x0044: AFE_DL1_CUR */
	uint32_t dl1_end;	/* 0x0048: AFE_DL1_END */
	uint32_t _rsv4[8];
	uint32_t conn_24bit;	/* 0x006c: AFE_CONN_24BIT */
	uint32_t _rsv5[0xe0];
	uint32_t apll1_cfg;	/* 0x03f0: AFE_APLL1_TUNER_CFG */
	uint32_t apll2_cfg;	/* 0x03f4: AFE_APLL2_TUNER_CFG */
} __attribute__((packed)) Mt8173I2sRegs;

/* AFE_I2S_CON1 */
enum {
	AFE_I2S_CON1_LOW_JITTER_CLOCK = 1 << 12,
	AFE_I2S_CON1_FORMAT_I2S = 1 << 3,
	AFE_I2S_CON1_WLEN_32BITS = 1 << 1,
	AFE_I2S_CON1_ENABLE = 1 << 0,
};

enum {
	AFE_I2S_RATE_MASK = 0xf,
	AFE_I2S_RATE_SHIFT = 8,
};

/* AFE_APLL_TUNER_CFG */
enum {
	AFE_APLL_TUNER_UPPER_BOUND = 0x80 << 8,
	AFE_APLL_TUNER_DIV = 0x3 << 4,
	AFE_APLL_TUNER_PLL1_SEL = 0x1 << 1,
	AFE_APLL_TUNER_PLL2_SEL = 0x2 << 1,
	AFE_APLL_TUNER_EN = 0x1 << 0,
};

/* APMIXEDSYS */
typedef struct {
	uint32_t _rsv0[5];
	uint32_t ap_pll_con5;		/* 0x0014: AP_PLL_CON5 */
	uint32_t _rsv1[0xa2];
	uint32_t apll1_con0;		/* 0x02a0: AUDIO_APLL1_CON0 */
	uint32_t apll1_con1;		/* 0x02a4: AUDIO_APLL1_CON1 */
	uint32_t apll1_con2;		/* 0x02a8: AUDIO_APLL1_CON2 */
	uint32_t _rsv2;
	uint32_t apll1_pwr_con0;	/* 0x02b0: AUDIO_APLL1_PWR_CON0 */
	uint32_t apll2_con0;		/* 0x02b4: AUDIO_APLL2_CON0 */
	uint32_t apll2_con1;		/* 0x02b8: AUDIO_APLL2_CON1 */
	uint32_t _rsv3;
	uint32_t apll2_con2;		/* 0x02c0: AUDIO_APLL2_CON2 */
	uint32_t apll2_pwr_con0;	/* 0x02c4: AUDIO_APLL2_PWR_CON0 */
} __attribute__((packed)) Mt8173ApMixedRegs;

/* TOPCKGEN */
typedef struct {
	uint32_t _rsv0;
	uint32_t dcm_cfg;	/* 0x0004: AUDIO_DCM_CFG */
	uint32_t _rsv1[0x1e];
	uint32_t clk_cfg4;	/* 0x0080: AUDIO_CLK_CFG_4 */
	uint32_t _rsv2[7];
	uint32_t clk_cfg6;	/* 0x00a0: AUDIO_CLK_CFG_6 */
	uint32_t _rsv3[3];
	uint32_t clk_cfg7;	/* 0x00b0: AUDIO_CLK_CFG_7 */
	uint32_t _rsv4[0x1b];
	uint32_t clk_auddiv0;	/* 0x0120: AUDIO_CLK_AUDDIV_0 */
	uint32_t clk_auddiv1;	/* 0x0124: AUDIO_CLK_AUDDIV_1 */
	uint32_t clk_auddiv2;	/* 0x0128: AUDIO_CLK_AUDDIV_2 */
	uint32_t clk_auddiv3;	/* 0x012c: AUDIO_CLK_AUDDIV_3 */
} __attribute__((packed)) Mt8173TopCkRegs;

enum {
	MTK_AFE_APLL1_CLK_FREQ = (22579200 * 8),
	MTK_AFE_APLL2_CLK_FREQ = (24576000 * 8),
};

typedef struct {
	SoundRouteComponent component;
	I2sOps ops;
	void *regs;
	uint32_t initialized;
	uint32_t channels;
	uint32_t rate;
} MtkI2s;

MtkI2s *new_mtk_i2s(uintptr_t base, uint32_t channels, uint32_t rate);

#endif /* __DRIVERS_BUS_I2S_MTK_H__ */

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2021 Mediatek Inc.
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

#ifndef __DRIVERS_BUS_I2S_MT8186_H__
#define __DRIVERS_BUS_I2S_MT8186_H__

/* AFE */
typedef struct {
	uint32_t top_con0;	/* 0x0000: AUDIO_TOP_CON0 */
	uint32_t _rsv0[3];
	uint32_t dac_con0;	/* 0x0010: AFE_DAC_CON0 */
	uint32_t dac_con1;	/* 0x0014: AFE_DAC_CON1 */
	uint32_t i2s_con0;	/* 0x0018: AFE_I2S_CON */
	uint32_t _rsv1[1];
	uint32_t conn0;		/* 0x0020: AFE_CONN0 */
	uint32_t conn1;		/* 0x0024: AFE_CONN1 */
	uint32_t conn2;		/* 0x0028: AFE_CONN2 */
	uint32_t _rsv2[2];
	uint32_t i2s_con1;	/* 0x0034: AFE_I2S_CON1 */
	uint32_t i2s_con2;	/* 0x0038: AFE_I2S_CON2 */
	uint32_t _rsv3[1];
	uint32_t i2s_con3;	/* 0x0040: AFE_I2S_CON3 */
	uint32_t _rsv4[1];
	uint32_t conn_24bit;	/* 0x0048: AFE_CONN_24BIT */
	uint32_t dl1_con0;	/* 0x004c: AFE_DL1_CON0 */
	uint32_t _rsv5[1];
	uint32_t dl1_base;	/* 0x0054: AFE_DL1_BASE */
	uint32_t _rsv6[1];
	uint32_t dl1_cur;	/* 0x005c: AFE_DL1_CUR */
	uint32_t _rsv7[1];
	uint32_t dl1_end;	/* 0x0064: AFE_DL1_END */
	uint32_t _rsv8[0xe2];
	uint32_t apll1_cfg;	/* 0x03f0: AFE_APLL1_TUNER_CFG */
	uint32_t apll2_cfg;	/* 0x03f4: AFE_APLL2_TUNER_CFG */
} __attribute__((packed)) MtkI2sRegs;

#define MTK_MEMIF_CHANNEL(r)	r->dl1_con0
#define MTK_MEMIF_RATE(r)	r->dl1_con0

enum {
	MTK_MEMIF_CHANNEL_SFT = 8,
	MTK_MEMIF_RATE_SFT = 24,
	MTK_MEMIF_ENABLE_SFT = 4,
};

#endif /* __DRIVERS_BUS_I2S_MT8186_H__ */

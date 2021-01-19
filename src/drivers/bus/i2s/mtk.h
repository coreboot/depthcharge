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
 */

#ifndef __DRIVERS_BUS_I2S_MTK_H__
#define __DRIVERS_BUS_I2S_MTK_H__

#include "drivers/bus/i2s/i2s.h"
#include "drivers/sound/route.h"

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
	uint32_t dl1_base;	/* 0x0040: AFE_DL1_BASE */
	uint32_t dl1_cur;	/* 0x0044: AFE_DL1_CUR */
	uint32_t dl1_end;	/* 0x0048: AFE_DL1_END */
	uint32_t i2s_con3;	/* 0x004c: AFE_I2S_CON3 */
	uint32_t _rsv4[7];
	uint32_t conn_24bit;	/* 0x006c: AFE_CONN_24BIT */
	uint32_t _rsv5[0xe0];
	uint32_t apll1_cfg;	/* 0x03f0: AFE_APLL1_TUNER_CFG */
	uint32_t apll2_cfg;	/* 0x03f4: AFE_APLL2_TUNER_CFG */
} __attribute__((packed)) MtkI2sRegs;

/* AFE_DAC_CON1 */
enum {
	AFE_DAC_CON1_DL1_CH_SHIFT = 21,
};

/* AFE_I2S_CON */
enum {
	AFE_I2S_CON0_LOW_JITTER_CLOCK = 1 << 12,
	AFE_I2S_CON0_FORMAT_I2S = 1 << 3,
	AFE_I2S_CON0_WLEN_32BITS = 1 << 1,
	AFE_I2S_CON0_ENABLE = 1 << 0,
};
/* AFE_I2S_CON1 */
enum {
	AFE_I2S_CON1_LOW_JITTER_CLOCK = 1 << 12,
	AFE_I2S_CON1_FORMAT_I2S = 1 << 3,
	AFE_I2S_CON1_WLEN_32BITS = 1 << 1,
	AFE_I2S_CON1_ENABLE = 1 << 0,
};

/* AFE_I2S_CON2 */
enum {
	AFE_I2S_CON2_LOW_JITTER_CLOCK = 1 << 12,
	AFE_I2S_CON2_FORMAT_I2S = 1 << 3,
	AFE_I2S_CON2_WLEN_32BITS = 1 << 1,
	AFE_I2S_CON2_ENABLE = 1 << 0,
};

/* AFE_I2S_CON3 */
enum {
	AFE_I2S_CON3_LOW_JITTER_CLOCK = 1 << 12,
	AFE_I2S_CON3_FORMAT_I2S = 1 << 3,
	AFE_I2S_CON3_WLEN_32BITS = 1 << 1,
	AFE_I2S_CON3_ENABLE = 1 << 0,
};

enum {
	AFE_I2S_RATE_MASK = 0xf,
	AFE_I2S_RATE_SHIFT = 8,
};

enum {
	AFE_I2S0, /* i2s0: input */
	AFE_I2S1, /* i2s1: output */
	AFE_I2S2, /* i2s2: input */
	AFE_I2S3, /* i2s3: output */
	AFE_I2S0_I2S1, /* combination */
	AFE_I2S2_I2S3, /* combination */
};

typedef struct {
	SoundRouteComponent component;
	I2sOps ops;
	void *regs;
	uint32_t initialized;
	uint32_t channels;
	uint32_t rate;
	uint32_t i2s_num;
} MtkI2s;

MtkI2s *new_mtk_i2s(uintptr_t base, uint32_t channels, uint32_t rate,
		    uint32_t i2s_num);

#endif /* __DRIVERS_BUS_I2S_MTK_H__ */

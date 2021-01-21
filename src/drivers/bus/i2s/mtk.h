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

#if CONFIG(DRIVER_BUS_I2S_MT8173)
#include "drivers/bus/i2s/mt8173.h"
#elif CONFIG(DRIVER_BUS_I2S_MT8183)
#include "drivers/bus/i2s/mt8183.h"
#elif CONFIG(DRIVER_BUS_I2S_MT8192)
#include "drivers/bus/i2s/mt8192.h"
#else
#error "Unsupported BUS I2S config for MediaTek"
#endif

/* AFE_I2S_CON0 */
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

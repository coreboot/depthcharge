/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2022 Mediatek Inc.
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

#ifndef __DRIVERS_BUS_I2S_MTK_V2_H__
#define __DRIVERS_BUS_I2S_MTK_V2_H__

#include "drivers/bus/i2s/i2s.h"
#include "drivers/sound/route.h"

#if CONFIG(DRIVER_BUS_I2S_MT8195)
#include "drivers/bus/i2s/mt8195.h"
#else
#error "Unsupported BUS I2S config for MediaTek"
#endif

enum {
	AFE_I2S_O1,	/* i2s1: output */
	AFE_I2S_O2,	/* i2s2: output */
	AFE_I2S_I1O1,	/* combination */
	AFE_I2S_I1O2,	/* combination */
	AFE_I2S_I2O1,	/* combination */
	AFE_I2S_I2O2,	/* combination */
};

typedef struct {
	SoundRouteComponent component;
	I2sOps ops;
	void *regs;
	uint32_t initialized;
	uint32_t channels;
	uint32_t rate;
	uint32_t word_len;
	uint32_t bit_len;
	uint32_t i2s_num;
} MtkI2s;

MtkI2s *new_mtk_i2s(uintptr_t base, uint32_t channels, uint32_t rate,
		    uint32_t word_len, uint32_t bit_len, uint32_t i2s_num);

#endif /* __DRIVERS_BUS_I2S_MTK_V2_H__ */

/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * rt5682s.h -- RealTek ALC5682i-VS ALSA SoC Audio driver
 *
 * Copyright 2021 Realtek Semiconductor Corp. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DRIVERS_SOUND_RT5682S_H__
#define __DRIVERS_SOUND_RT5682S_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/route.h"

typedef struct {
	SoundRouteComponent component;

	I2cOps *i2c;
	uint8_t chip;
	uint32_t mclk;
	uint32_t lrclk;
} rt5682sCodec;

rt5682sCodec *new_rt5682s_codec(I2cOps *i2c, uint8_t chip, uint32_t mclk,
				uint32_t lrclk);

#endif /* __DRIVERS_SOUND_RT5682S_H__ */

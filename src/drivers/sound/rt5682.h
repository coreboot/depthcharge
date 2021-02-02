/*
 * rt5682.h -- RealTek ALC5682 ALSA SoC Audio driver
 *
 * Copyright 2020 Realtek Semiconductor Corp. All rights reserved.
 * Copyright 2020 Advanced Micro Devices, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DRIVERS_SOUND_RT5682_H__
#define __DRIVERS_SOUND_RT5682_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/route.h"

typedef struct {
	SoundRouteComponent component;

	I2cOps *i2c;
	uint8_t chip;
	uint32_t mclk;
	uint32_t lrclk;
} rt5682Codec;

rt5682Codec *new_rt5682_codec(I2cOps *i2c, uint8_t chip, uint32_t mclk,
			      uint32_t lrclk);

#endif /* __DRIVERS_SOUND_RT5682_H__ */

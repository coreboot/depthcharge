/*
 * max98927.h -- Maxim Integrated 98927
 *
 * Copyright 2016 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DRIVERS_SOUND_MAX98927_H__
#define __DRIVERS_SOUND_MAX98927_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/route.h"

typedef struct
{
	SoundRouteComponent component;

	I2cOps *i2c;
	uint8_t chip;

	int sample_rate;
	int bits_per_sample;
	int lr_frame_size;
} Max98927Codec;

Max98927Codec *new_max98927_codec(I2cOps *i2c, uint8_t chip,
				  int bits_per_sample, int sample_rate,
				  int lr_frame_size);

#endif /* __DRIVERS_SOUND_MAX98927_H__ */

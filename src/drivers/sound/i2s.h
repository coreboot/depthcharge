/*
 * Copyright 2013 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#ifndef __DRIVERS_SOUND_I2S_H__
#define __DRIVERS_SOUND_I2S_H__

#include "drivers/sound/sound.h"

typedef struct
{
	SoundOps ops;

	int bits_per_sample;
	int sample_rate;
	int channels;
	int lr_frame_size;
	uint16_t volume;
} I2sSource;

I2sSource *new_i2s_source(int bits_per_sample, int sample_rate, int channels,
			  int lr_frame_size, uint16_t volume);

#endif /* __DRIVERS_SOUND_I2S_H__ */

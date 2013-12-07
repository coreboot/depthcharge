/*
 * Copyright 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2012 Samsung Electronics
 * R. Chandrasekar <rcsekar@samsung.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <assert.h>
#include <libpayload.h>
#include <stdint.h>

#include "base/container_of.h"
#include "config.h"
#include "drivers/bus/i2s/i2s.h"
#include "drivers/sound/i2s.h"

// Generates square wave sound data for 1 second.
static void sound_square_wave(uint16_t *data, int channels,
			      int sample_rate, uint32_t freq, uint16_t volume)
{
	assert(freq);

	const int period = sample_rate / freq;
	const int half = period / 2;

	int samples = sample_rate;

	while (samples) {
		for (int i = 0; samples && i < half; samples--, i++) {
			for (int j = 0; j < channels; j++)
				*data++ = volume;
		}
		for (int i = 0; samples && i < period - half; samples--, i++) {
			for (int j = 0; j < channels; j++)
				*data++ = -volume;
		}
	}
}

static void finish_delay(uint64_t start, uint32_t msec)
{
	uint32_t passed = timer_us(start) / 1000;
	mdelay(msec - passed);
}

static int i2s_source_play(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	I2sSource *source = container_of(me, I2sSource, ops);

	const int channels = source->channels;
	const int sample_rate = source->sample_rate;

	// Prepare a buffer for 1 second of sound.
	int bytes = sample_rate * channels * sizeof(uint16_t);
	uint32_t *data = xmalloc(bytes);

	sound_square_wave((uint16_t *)data, channels, sample_rate, frequency,
			  source->volume);

	uint64_t start = timer_us(0);

	while (msec >= 1000) {
		if (source->i2s->send(source->i2s, data,
				      bytes / sizeof(uint32_t))) {
			finish_delay(start, msec);
			free(data);
			return 1;
		}
		msec -= 1000;
	}
	if (msec) {
		int size = (bytes * msec) / (sizeof(uint32_t) * 1000);
		if (source->i2s->send(source->i2s, data, size)) {
			finish_delay(start, msec);
			free(data);
			return 1;
		}
	}

	free(data);
	return 0;
}

I2sSource *new_i2s_source(I2sOps *i2s, int sample_rate, int channels,
			  uint16_t volume)
{
	I2sSource *source = xzalloc(sizeof(*source));

	source->ops.play = &i2s_source_play;

	source->i2s = i2s;

	source->sample_rate = sample_rate;
	source->channels = channels;
	source->volume = volume;

	return source;
}

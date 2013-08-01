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

#include "base/list.h"
#include "base/time.h"
#include "config.h"
#include "drivers/bus/i2s/i2s.h"
#include "drivers/sound/i2s.h"
#include "drivers/timer/timer.h"

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

	const int bits_per_sample = source->bits_per_sample;
	const int channels = source->channels;
	const int bit_frame_size = bits_per_sample * channels;
	const int sample_rate = source->sample_rate;
	const int lr_frame_size = source->lr_frame_size;

	// Prepare a buffer for 1 second of sound.
	int bytes = sample_rate * channels * sizeof(uint16_t);
	uint32_t *data = malloc(bytes);
	if (data == NULL) {
		printf("%s: malloc failed\n", __func__);
		return 1;
	}

	sound_square_wave((uint16_t *)data, channels, sample_rate, frequency,
			  source->volume);

	i2s_transfer_init(lr_frame_size, bits_per_sample, bit_frame_size);

	uint64_t start = timer_us(0);

	while (msec >= 1000) {
		if (i2s_send(data, bytes / sizeof(uint32_t))) {
			finish_delay(start, msec);
			free(data);
			return 1;
		}
		msec -= 1000;
	}
	if (msec) {
		int size = (bytes * msec) / (sizeof(uint32_t) * 1000);
		if (i2s_send(data, size)) {
			finish_delay(start, msec);
			free(data);
			return 1;
		}
	}

	free(data);
	return 0;
}

I2sSource *new_i2s_source(int bits_per_sample, int sample_rate, int channels,
			  int lr_frame_size, uint16_t volume)
{
	I2sSource *source = malloc(sizeof(*source));
	if (!source) {
		printf("Failed to allocate I2S source structure.\n");
		return NULL;
	}
	memset(source, 0, sizeof(*source));

	source->ops.play = &i2s_source_play;

	source->bits_per_sample = bits_per_sample;
	source->sample_rate = sample_rate;
	source->channels = channels;
	source->lr_frame_size = lr_frame_size;
	source->volume = volume;

	return source;
}

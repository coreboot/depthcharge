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

#include "base/time.h"
#include "config.h"
#include "drivers/bus/i2s/i2s.h"
#include "drivers/sound/i2s.h"
#include "drivers/timer/timer.h"

// Generates square wave sound data for 1 second.
static void sound_square_wave(uint16_t *data, int channels,
			      int sample_rate, uint32_t freq)
{
	assert(freq);

	const uint16_t amplitude = CONFIG_DRIVER_SOUND_I2S_VOLUME;
	const int period = sample_rate / freq;
	const int half = period / 2;

	int samples = sample_rate;

	while (samples) {
		for (int i = 0; samples && i < half; samples--, i++) {
			for (int j = 0; j < channels; j++)
				*data++ = amplitude;
		}
		for (int i = 0; samples && i < period - half; samples--, i++) {
			for (int j = 0; j < channels; j++)
				*data++ = -amplitude;
		}
	}
}

int sound_start(uint32_t frequency)
{
	return -1;
}

int sound_stop(void)
{
	return -1;
}

static void finish_delay(uint64_t start, uint32_t msec)
{
	uint32_t passed = timer_time_in_us(start) / 1000;
	mdelay(msec - passed);
}

int sound_play(uint32_t msec, uint32_t frequency)
{
	const int bits_per_sample = 16;
	const int channels = 2;
	const int bit_frame_size = bits_per_sample * channels;
	const int sample_rate = CONFIG_DRIVER_SOUND_I2S_SAMPLE_RATE;
	const int lr_frame_size = CONFIG_DRIVER_SOUND_I2S_LR_FRAME_SIZE;

	static int codec_initialized = 0;
	if (!codec_initialized) {
		if (codec_init(bits_per_sample, sample_rate, lr_frame_size))
			return 1;
		codec_initialized = 1;
	}

	// Prepare a buffer for 1 second of sound.
	int bytes = sample_rate * channels * sizeof(uint16_t);
	uint32_t *data = malloc(bytes);
	if (data == NULL) {
		printf("%s: malloc failed\n", __func__);
		return 1;
	}

	sound_square_wave((uint16_t *)data, channels, sample_rate, frequency);

	i2s_transfer_init(lr_frame_size, bits_per_sample, bit_frame_size);

	uint64_t start = timer_time_in_us(0);

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

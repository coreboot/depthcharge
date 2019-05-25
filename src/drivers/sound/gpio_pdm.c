/*
 * Copyright (C) 2015 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <libpayload.h>
#include <stdint.h>

#include "base/container_of.h"
#include "drivers/sound/gpio_pdm.h"

/* Square wave with 1 channel */
static void pdm_square_wave(uint16_t *data, unsigned samples,
			    uint32_t freq, uint16_t volume)
{
	unsigned period = samples / freq;
	unsigned half = period / 2;

	while (samples) {
		for (int i = 0; samples && i < half; samples--, i++)
			*data++ = volume;
		for (int i = 0; samples && i < period - half; samples--, i++)
			*data++ = -volume;
	}
}

/* Send PDM data bit, left and right channels */
static void gpio_pdm_send_bit(GpioPdm *pdm, int bit)
{
	switch (pdm->channels) {
	case 1:
		/* Mono: send data on both clock edges */
		gpio_set(pdm->data_gpio, bit);

		/* Toggle Clock */
		gpio_set(pdm->clock_gpio, gpio_get(pdm->clock_gpio) ^ 1);
		break;

	case 2:
		/* Stereo: left channel on rising edge */
		gpio_set(pdm->data_gpio, bit);
		gpio_set(pdm->clock_gpio, 1);

		/* Stereo: right channel on falling edge */
		gpio_set(pdm->data_gpio, bit);
		gpio_set(pdm->clock_gpio, 0);
		break;

	default:
		printf("%s: Unsupported channel count %d\n",
		       __func__, pdm->channels);
	}
}

/* Start PDM clock with no data */
static void gpio_pdm_clock_start(GpioPdm *pdm)
{
	for (int i = 0; i < pdm->clock_start; i++)
		gpio_pdm_send_bit(pdm, 0);
}

/* Send PDM data */
static void gpio_pdm_send(GpioPdm *pdm, uint16_t *data, size_t length)
{
	for (; length > 0; length--, data++)
		for (int i = 0; i < 16; i++)
			gpio_pdm_send_bit(pdm, !!(*data & (1 << i)));
}

static int gpio_pdm_play(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	GpioPdm *pdm = container_of(me, GpioPdm, ops);

	/* Prepare a buffer for 1 second of sound. */
	unsigned bytes = pdm->sample_rate * sizeof(uint16_t);
	uint16_t *data = xmalloc(bytes);

	/* Generate a square wave tone with one channel */
	pdm_square_wave(data, pdm->sample_rate, frequency, pdm->volume);

	/* Start the PDM clock */
	gpio_pdm_clock_start(pdm);

	/* Send 1 second chunks */
	while (msec >= 1000) {
		gpio_pdm_send(pdm, data, bytes / sizeof(uint16_t));
		msec -= 1000;
	}

	/* Send remaining portion of a second */
	if (msec) {
		size_t len = (bytes * msec) / (sizeof(uint16_t) * 1000);
		gpio_pdm_send(pdm, data, len);
	}

	free(data);
	return 0;
}

GpioPdm *new_gpio_pdm(GpioOps *clock_gpio, GpioOps *data_gpio,
		      unsigned clock_start, unsigned sample_rate,
		      unsigned channels, uint16_t volume)
{
	GpioPdm *pdm = xzalloc(sizeof(*pdm));

	pdm->ops.play    = &gpio_pdm_play;

	pdm->clock_gpio  = clock_gpio;
	pdm->data_gpio   = data_gpio;

	pdm->clock_start = clock_start;
	pdm->sample_rate = sample_rate;
	pdm->channels    = channels;
	pdm->volume      = volume;

	return pdm;
}

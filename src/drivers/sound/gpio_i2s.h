/*
 * Copyright (C) 2015 Google Inc.
 * Copyright (C) 2015 Intel Corporation.
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

#include "drivers/gpio/gpio.h"
#include "drivers/sound/sound.h"

typedef struct GpioI2s {
	SoundOps ops;

	/* GPIO to use for I2S BCLK */
	GpioOps *bclk_gpio;

	/* GPIO to use for I2S SFRM */
	GpioOps *sfrm_gpio;

	/* GPIO to use for I2S DATA */
	GpioOps *data_gpio;

	/* Based on the toggle rate of GPIO*/
	uint16_t sample_rate;

	/* Number of channels */
	uint8_t channels;

	/* Amplitude for square wave */
	int16_t volume;

	/* i2s send */
	void (*send)(struct GpioI2s *i2s, uint16_t *data, size_t length);
} GpioI2s;

GpioI2s *new_gpio_i2s(GpioOps *bclk_gpio, GpioOps *sfrm_gpio,
		      GpioOps *data_gpio, uint16_t sample_rate,
		      uint8_t channels, int16_t volume);

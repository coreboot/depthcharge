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

#ifndef __DRIVERS_SOUND_GPIO_PDM_H__
#define __DRIVERS_SOUND_GPIO_PDM_H__

#include "drivers/gpio/gpio.h"
#include "drivers/sound/sound.h"

typedef struct
{
	SoundOps ops;

	/* GPIO to use for PDM Clock */
	GpioOps *clock_gpio;
	/* GPIO to use for PDM Data */
	GpioOps *data_gpio;

	/* Number of empty clock cycles to send before data */
	unsigned clock_start;
	/* Based on codec and how fast the platform can toggle GPIOs */
	unsigned sample_rate;
	/* Supports Mono=1 Stereo=2 */
	unsigned channels;
	/* Amplitude for square wave tone */
	uint16_t volume;
} GpioPdm;

GpioPdm *new_gpio_pdm(GpioOps *clock_gpio, GpioOps *data_gpio,
		      unsigned clock_start, unsigned sample_rate,
		      unsigned channels, uint16_t volume);

#endif /* __DRIVERS_SOUND_GPIO_PDM_H__ */

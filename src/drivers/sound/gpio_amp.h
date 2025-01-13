/*
 * gpio_amp.h -- Audio driver for GPIO based amplifier
 *
 * Copyright 2015 Google LLC
 * Copyright (C) 2015 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_SOUND_GPIO_AMP_H__
#define __DRIVERS_SOUND_GPIO_AMP_H__

#include "drivers/gpio/gpio.h"
#include "drivers/sound/route.h"

typedef struct
{
	SoundRouteComponent component;
	GpioOps *enable_speaker;

	/* Delay after enabling the amplifier */
	unsigned int enable_delay_us;
} GpioAmpCodec;

GpioAmpCodec *new_gpio_amp_codec(GpioOps *ops);
GpioAmpCodec *new_gpio_amp_codec_with_delay(GpioOps *ops,
					    unsigned int enable_delay_us);

#endif /* __DRIVERS_SOUND_GPIO_AMP_H__ */

/*
 * Copyright 2016 Google Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/sound/gpio_buzzer.h"

static int buzzer_start(SoundOps *me, uint32_t frequency)
{
	GpioBuzzer *buzzer = container_of(me, GpioBuzzer, ops);

	if (!frequency)
		return -1;

	buzzer->gpio->set(buzzer->gpio, 1);

	return 0;
}

static int buzzer_stop(SoundOps *me)
{
	GpioBuzzer *buzzer = container_of(me, GpioBuzzer, ops);

	buzzer->gpio->set(buzzer->gpio, 0);

	return 0;
}

static int buzzer_play(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	sound_start(frequency);
	mdelay(msec);
	sound_stop();

	return 0;
}

GpioBuzzer *new_gpio_buzzer_sound(GpioOps *gpio)
{
	GpioBuzzer *buzzer = xzalloc(sizeof(*buzzer));

	buzzer->gpio = gpio;
	buzzer->ops.start = buzzer_start;
	buzzer->ops.stop = buzzer_stop;
	buzzer->ops.play = buzzer_play;

	return buzzer;
}

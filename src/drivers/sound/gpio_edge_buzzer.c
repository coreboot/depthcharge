/*
 * Copyright 2017 Google Inc.
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
#include "drivers/sound/gpio_edge_buzzer.h"

static int buzzer_play(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	GpioEdgeBuzzer *buzzer = container_of(me, GpioEdgeBuzzer, ops);
	u64 start;
	int i = 0;

	if (!frequency)
		return -1;

	/* Using frequency ~2.7kHz, which is peak loudness */
	start = timer_us(0);
	while (timer_us(start)/1000 < msec) {
		buzzer->gpio->set(buzzer->gpio, ++i & 1);
		udelay(185);
	}
	buzzer->gpio->set(buzzer->gpio, 0);

	return 0;
}

GpioEdgeBuzzer *new_gpio_edge_buzzer(GpioOps *gpio)
{
	GpioEdgeBuzzer *buzzer = xzalloc(sizeof(*buzzer));

	buzzer->gpio = gpio;
	buzzer->ops.play = buzzer_play;

	return buzzer;
}

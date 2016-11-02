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

#ifndef __DRIVERS_SOUND_GPIO_BUZZER_H__
#define __DRIVERS_SOUND_GPIO_BUZZER_H__

#include "drivers/gpio/gpio.h"
#include "drivers/sound/sound.h"

typedef struct
{
	SoundOps ops;
	GpioOps *gpio;
} GpioBuzzer;

GpioBuzzer *new_gpio_buzzer_sound(GpioOps *gpio);

#endif /* __DRIVERS_SOUND_GPIO_BUZZER_H__*/

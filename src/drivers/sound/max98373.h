/*
 * max98373.h -- Maxim Integrated 98373 using DSP & I2S driven approach
 *
 * Copyright 2020 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DRIVERS_SOUND_MAX98373_H__
#define __DRIVERS_SOUND_MAX98373_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/bus/i2s/intel_common/i2s.h"
#include "drivers/gpio/gpio.h"
#include "drivers/sound/max98373_common_regs.h"
#include "drivers/sound/route.h"
#include "drivers/sound/sound.h"

typedef struct
{
	SoundOps ops;
	I2cOps *i2c;
	uint8_t chip;
	GpioOps *bclk_gpio;
	GpioOps *lrclk_gpio;
	int sample_rate;
	SoundRouteComponent component;
} Max98373Codec;

Max98373Codec *new_max98373_tone_generator(I2cOps *i2c, uint8_t chip,
					   int sample_rate,
					   GpioOps *bclk_gpio,
					   GpioOps *lrclk_gpio);

Max98373Codec *new_max98373_codec(I2cOps *i2c, uint8_t chip);
extern const I2sSettings max98373_settings;
#endif /* __DRIVERS_SOUND_MAX98373_H__ */

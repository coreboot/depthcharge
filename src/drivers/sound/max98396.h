/*
 * max98396.h -- Maxim Integrated 98396
 *
 * Copyright 2022 Google LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DRIVERS_SOUND_MAX98396_H__
#define __DRIVERS_SOUND_MAX98396_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/bus/i2s/intel_common/i2s.h"
#include "drivers/sound/route.h"
#include <inttypes.h>

/* Max98396 General Control Registers */
enum {
	MAX396_REG_SOFT_RESET		= 0x2000,
	MAX396_REG_PCM_MODE_CONFIG	= 0x2041,
	MAX396_REG_SAMPLE_RATE_1	= 0x2043,
	MAX396_REG_DATA_INPUT_EN	= 0X205E,
	MAX396_REG_AMP_VOLUME		= 0x2090,
	MAX396_REG_AMP_GAIN		= 0x2091,
	MAX396_REG_DSP_CONFIG		= 0x2092,
	MAX396_REG_TONE_CONFIG		= 0x2083,
	MAX396_REG_TONE_ENABLE		= 0x208F,
	MAX396_REG_SPEAKER_PATH		= 0x20AF,
	MAX396_REG_CLOCK_MONITOR	= 0x203F,
	MAX396_REG_GLOBAL_ENABLE	= 0x210F,
};

/* REG_DSP_CONFIG */
enum {
	SPK_SAFE_EN_MASK    = 0x20,
};

#define TONE_CONFIG_1K	0x04

typedef struct
{
	SoundRouteComponent component;

	I2cOps *i2c;
	uint8_t chip;
} Max98396Codec;

Max98396Codec *new_max98396_codec(I2cOps *i2c, uint8_t chip);
extern const I2sSettings max98396_settings;
#endif /* __DRIVERS_SOUND_MAX98396_H__ */

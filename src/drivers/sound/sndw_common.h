// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2025 Google LLC.
 */

#ifndef __DRIVERS_SOUND_SNDW_COMMON_H__
#define __DRIVERS_SOUND_SNDW_COMMON_H__

#include "drivers/bus/soundwire/soundwire.h"
#include "drivers/sound/sound.h"

typedef struct {
	SoundOps ops;
	SndwOps *sndw;
	uint32_t beep_duration;
} SoundDevice_sndw;

#endif

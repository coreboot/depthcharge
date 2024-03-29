// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2023 Google LLC.
 */

#ifndef __DRIVERS_SOUND_MAX98363_SNDW_H__
#define __DRIVERS_SOUND_MAX98363_SNDW_H__

#include "drivers/bus/soundwire/soundwire.h"
#include "drivers/sound/max98363_common_regs.h"
#include "drivers/sound/sound.h"

typedef struct {
	SoundOps ops;
	SndwOps *sndw;
	uint32_t beepduration;
} Max98363sndw;

/* new_max98363_sndw - new structure for Soundwire Max98363 codec. */
Max98363sndw *new_max98363_sndw(SndwOps *sndw, uint32_t beepduration);

#endif
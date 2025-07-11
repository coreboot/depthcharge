/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright 2025 Google LLC.
 */
#ifndef __DRIVERS_SOUND_RT1320_SNDW_H__
#define __DRIVERS_SOUND_RT1320_SNDW_H__

#include "drivers/bus/soundwire/soundwire.h"
#include "drivers/sound/rt1320_common_regs.h"
#include "drivers/sound/sound.h"

/*
 * This driver supports both rt1320 amplifier.
 */
typedef struct {
	SoundOps ops;
	SndwOps *sndw;
	uint32_t beep_duration;
} rt1320sndw;

/* new_rt1320_sndw - new structure for Soundwire rt1320 amplifier. */
rt1320sndw *new_rt1320_sndw(SndwOps *sndw, uint32_t beep_duration);
#endif

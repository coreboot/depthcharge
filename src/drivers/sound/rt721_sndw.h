// SPDX-License-Identifier: GPL-2.0

#ifndef __DRIVERS_SOUND_RT721_SNDW_H__
#define __DRIVERS_SOUND_RT721_SNDW_H__

#include "drivers/bus/soundwire/soundwire.h"
#include "drivers/sound/rt721_common_regs.h"
#include "drivers/sound/sound.h"

typedef struct {
	SoundOps ops;
	SndwOps *sndw;
	uint32_t beep_duration;
} rt721sndw;

/* new_rt721_sndw - new structure for Soundwire rt721 codec. */
rt721sndw *new_rt721_sndw(SndwOps *sndw, uint32_t beep_duration);

#endif

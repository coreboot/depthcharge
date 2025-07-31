
// SPDX-License-Identifier: GPL-2.0
#ifndef __DRIVERS_SOUND_RT721_SNDW_H__
#define __DRIVERS_SOUND_RT721_SNDW_H__

#include "drivers/bus/soundwire/soundwire.h"
#include "drivers/sound/rt7xx_common_regs.h"
#include "drivers/sound/sndw_common.h"
#include "drivers/sound/sound.h"

extern const sndw_codec_id rt712_id;
extern const sndw_codec_id rt721_id;
extern const sndw_codec_id rt722_id;

/* new_rt7xx_sndw - new structure for Soundwire rt721 and rt712 codecs. */
SoundDevice_sndw *new_rt7xx_sndw(SndwOps *sndw, uint32_t beep_duration);
#endif

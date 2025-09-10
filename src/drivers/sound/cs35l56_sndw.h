/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright 2025 Google LLC.
 */
#ifndef __DRIVERS_SOUND_CS35L56_SNDW_H__
#define __DRIVERS_SOUND_CS35L56_SNDW_H__

#include "drivers/bus/soundwire/soundwire.h"
#include "drivers/sound/cs35l56_common_regs.h"
#include "drivers/sound/sndw_common.h"
#include "drivers/sound/sound.h"

extern const sndw_codec_id cs35l56_id;

/* new_cs35l56_sndw - new structure for Soundwire cs35l56 codec. */
SoundDevice_sndw *new_cs35l56_sndw(SndwOps *sndw, uint32_t beep_duration);
#endif

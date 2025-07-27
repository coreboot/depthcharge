// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Intel Corporation.
 * Copyright 2021 Google LLC.
 */

#ifndef __DRIVERS_SOUND_MAX98373_SNDW_H__
#define __DRIVERS_SOUND_MAX98373_SNDW_H__

#include "drivers/bus/soundwire/soundwire.h"
#include "drivers/sound/max98373_common_regs.h"
#include "drivers/sound/sndw_common.h"
#include "drivers/sound/sound.h"

extern const sndw_codec_id max98373_id;

/* new_max98373_sndw - new structure for Soundwire Max98373 codec. */
SoundDevice_sndw *new_max98373_sndw(SndwOps *sndw, uint32_t beep_duration);

#endif

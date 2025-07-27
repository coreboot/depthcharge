
// SPDX-License-Identifier: GPL-2.0
#ifndef __DRIVERS_SOUND_RT721_SNDW_H__
#define __DRIVERS_SOUND_RT721_SNDW_H__

#include "drivers/bus/soundwire/soundwire.h"
#include "drivers/sound/rt7xx_common_regs.h"
#include "drivers/sound/sndw_common.h"
#include "drivers/sound/sound.h"

/*
 * This driver supports both RT712 and RT721 codecs.
 */

#if CONFIG(DRIVER_SOUND_RT712_SNDW)
#define RT_7XX_CODEC_PART1_ID 0x07
#define RT_7XX_CODEC_PART2_ID 0x12
#elif CONFIG(DRIVER_SOUND_RT721_SNDW)
#define RT_7XX_CODEC_PART1_ID 0x07
#define RT_7XX_CODEC_PART2_ID 0x21
#elif CONFIG(DRIVER_SOUND_RT722_SNDW)
#define RT_7XX_CODEC_PART1_ID 0x07
#define RT_7XX_CODEC_PART2_ID 0x22
#endif

/* new_rt7xx_sndw - new structure for Soundwire rt721 and rt712 codecs. */
SoundDevice_sndw *new_rt7xx_sndw(SndwOps *sndw, uint32_t beep_duration);
#endif

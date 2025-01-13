/*
 * Copyright 2013 Google LLC
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#ifndef __DRIVERS_SOUND_SOUND_H__
#define __DRIVERS_SOUND_SOUND_H__

#include <stdint.h>

typedef struct SoundOps
{
	int (*start)(struct SoundOps *me, uint32_t frequency);
	int (*stop)(struct SoundOps *me);
	int (*play)(struct SoundOps *me, uint32_t msec, uint32_t frequency);
	int (*set_volume)(struct SoundOps *me, uint32_t volume);
} SoundOps;

void sound_set_ops(SoundOps *ops);

int sound_start(uint32_t frequency);
int sound_stop(void);
int sound_play(uint32_t msec, uint32_t frequency);
/*
 * An API which uses a callback optionally provided by the driver. The volume
 * parameter is an abstract number in 0..100 range.
 */
int sound_set_volume(uint32_t volume);

#endif /* __DRIVERS_SOUND_SOUND_H__ */

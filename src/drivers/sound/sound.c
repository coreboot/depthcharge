/*
 * Copyright 2013 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <assert.h>
#include <libpayload.h>

#include "drivers/sound/sound.h"

static SoundOps *sound_ops;

void sound_set_ops(SoundOps *ops)
{
	die_if(sound_ops, "%s: Sound ops already set.\n", __func__);
	sound_ops = ops;
}

int sound_start(uint32_t frequency)
{
	if (!sound_ops) {
		printf("%s: No sound ops set.\n", __func__);
		return 1;
	}
	assert(sound_ops->start);

	return sound_ops->start(sound_ops, frequency);
}

int sound_stop(void)
{
	if (!sound_ops) {
		printf("%s: No sound ops set.\n", __func__);
		return 1;
	}
	assert(sound_ops->stop);

	return sound_ops->stop(sound_ops);
}

int sound_play(uint32_t msec, uint32_t frequency)
{
	if (!sound_ops) {
		printf("%s: No sound ops set.\n", __func__);
		return 1;
	}
	assert(sound_ops->play);

	return sound_ops->play(sound_ops, msec, frequency);
}

int sound_set_volume(uint32_t volume)
{
	if (!sound_ops) {
		printf("%s: No sound ops set.\n", __func__);
		return 1;
	}
	if (!sound_ops->set_volume) {
		printf("%s: No volume control provided.\n", __func__);
		return 1;
	}

	if (volume > 100) {
		printf("%s: Volume level must not exceed 100.\n", __func__);
		return 1;
	}

	return sound_ops->set_volume(sound_ops, volume);
}

/*
 * Copyright 2013 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <libpayload.h>

#include "drivers/sound/sound.h"

static SoundOps *sound_ops;

int sound_set_ops(SoundOps *ops)
{
	if (sound_ops) {
		printf("%s: Sound ops already set.\n", __func__);
		return -1;
	}
	sound_ops = ops;
	return 0;
}

int sound_start(uint32_t frequency)
{
	if (!sound_ops) {
		printf("%s: No sound ops set.\n", __func__);
		return 1;
	}
	if (!sound_ops->start)
		return -1;
	return sound_ops->start(sound_ops, frequency);
}

int sound_stop(void)
{
	if (!sound_ops) {
		printf("%s: No sound ops set.\n", __func__);
		return 1;
	}
	if (!sound_ops->stop)
		return -1;
	return sound_ops->stop(sound_ops);
}

int sound_play(uint32_t msec, uint32_t frequency)
{
	if (!sound_ops) {
		printf("%s: No sound ops set.\n", __func__);
		return 1;
	}
	if (!sound_ops->play)
		return -1;
	return sound_ops->play(sound_ops, msec, frequency);
}

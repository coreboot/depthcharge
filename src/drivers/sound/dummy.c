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

int sound_start(uint32_t frequency)
{
	return -1;
}

int sound_stop(void)
{
	return -1;
}

int sound_play(uint32_t msec, uint32_t frequency)
{
	printf("Beep beep!\n");
	return 0;
}

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

void enable_beep(uint32_t frequency)
{
	printf("Enable beep at %d Hz.\n", frequency);
}

void disable_beep(void)
{
	printf("Disable beep.\n");
}

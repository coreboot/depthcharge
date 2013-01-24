/*
 * Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
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

void enable_beep(uint32_t frequency);
void disable_beep(void);

#endif /* __DRIVERS_SOUND_SOUND_H__ */

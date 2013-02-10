/*
 * Copyright 2011 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

/* HDA codec interface for Chrome OS verified boot */

#ifndef __DRIVERS_SOUND_HDA_CODEC_H__
#define __DRIVERS_SOUND_HDA_CODEC_H__

#include <stdint.h>

/* Beep control */
void enable_beep(uint32_t frequency);
void disable_beep(void);

#endif /* __DRIVERS_SOUND_HDA_CODEC_H__ */

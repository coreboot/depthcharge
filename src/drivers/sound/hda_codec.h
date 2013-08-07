/*
 * Copyright 2013 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#ifndef __DRIVERS_SOUND_HDA_CODEC_H__
#define __DRIVERS_SOUND_HDA_CODEC_H__

#include "drivers/sound/sound.h"

typedef struct
{
	SoundOps ops;
	int beep_nid_override;
} HdaCodec;

HdaCodec *new_hda_codec(void);
void set_hda_beep_nid_override(HdaCodec *codec, int nid);

#endif /* __DRIVERS_SOUND_HDA_CODEC_H__ */

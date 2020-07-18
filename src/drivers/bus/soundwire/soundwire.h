// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020, Intel Corporation.
 * Copyright 2020 Google LLC.
 */

#ifndef __DRIVERS_BUS_SOUNDWIRE_SOUNDWIRE_H__
#define __DRIVERS_BUS_SOUNDWIRE_SOUNDWIRE_H__

#include "drivers/bus/soundwire/cavs_2_5-sndwregs.h"
#include "drivers/bus/soundwire/mipi-sndwregs.h"

#define RETRY_COUNT		1000
#define DEBUG_SNDW		0

/*
 * Soundwire ops
 * sndw_enable() - Enables the HDA/DSP/SNDW.
 * sndw_sendwack() - Send the txcmds and receive the response.
 * sndw_disable() - Disable the soundwire interface by resetting the DSP.
 */
typedef struct SndwOps {
	int (*sndw_enable)(struct SndwOps *me, sndw_codec_info *codecinfo);
        int (*sndw_sendwack)(uint32_t sndwlinkaddr, sndw_cmd txcmd, uint32_t deviceindex);
	int (*sndw_disable)(struct SndwOps *me);
} SndwOps;

typedef struct {
        /* Soundwire ops structure.*/
        SndwOps ops;
        /* Intel HD Audio Lower Base Address.*/
        uintptr_t hdabar;
        /* Audio DSP Lower Base Address.*/
        uintptr_t dspbar;
	/* Sndw Link Number */
	int sndwlinkindex;
	/* Sndw Link address */
	uint32_t sndwlinkaddr;
} Soundwire;

/*
 * new_soundwire - Allocate new Soundwire data structures.
 * @Sndwlinkindex: Sndw Link Number
 */
Soundwire *new_soundwire(int sndwlinkindex);

#endif

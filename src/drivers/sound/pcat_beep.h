/*
 * Copyright (c) 2013 Sage Electronic Engineering, LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#ifndef __DRIVERS_SOUND_PCAT_BEEP_H__
#define __DRIVERS_SOUND_PCAT_BEEP_H__

#include "drivers/sound/sound.h"

typedef struct
{
	SoundOps ops;
	int timer_init_done;
} PcAtBeep;

PcAtBeep *new_pcat_beep(void);

/* Legacy 8254 PIT */
enum {
	PitBaseAddr = 0x40,
	PitT0 = 0x0,		/* PIT channel 0 count/status */
	PitT1 = 0x1,		/* PIT channel 1 count/status */
	PitT2 = 0x2,		/* PIT channel 2 count/status */
	PitCommand = 0x3	/* PIT mode control, latch and read back */
};

/* PIT Command Register Bit Definitions */
enum {
	PitCmdCtr0 = 0x0,	/* Select PIT counter 0 */
	PitCmdCtr1 = 0x40,	/* Select PIT counter 1 */
	PitCmdCtr2 = 0x80,	/* Select PIT counter 2 */
};

enum {
	PitCmdLatch = 0x0,	/* Counter Latch Command */
	PitCmdLow = 0x10,	/* Access counter bits 7-0 */
	PitCmdHigh = 0x20,	/* Access counter bits 15-8 */
	PitCmdBoth = 0x30,	/* Access counter bits 15-0 in two accesses */
};

enum {
	PitCmdMode0 = 0x0,	/* Select mode 0 */
	PitCmdMode1 = 0x02,	/* Select mode 1 */
	PitCmdMode2 = 0x04,	/* Select mode 2 */
	PitCmdMode3 = 0x06,	/* Select mode 3 */
	PitCmdMode4 = 0x08,	/* Select mode 4 */
	PitCmdMode5 = 0x0a,	/* Select mode 5 */
};

static const uint16_t PitTimer2Value = 0x0a8e; /* 440Hz */
static const uint16_t PitPortBAddr = 0x61;
static const uint16_t PitPortBBeepEnable = 0x3;
static const uint32_t PitHz = 1193180;

#endif /* __DRIVERS_SOUND_PCAT_BEEP_H__*/

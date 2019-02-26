/*
  * Copyright (c) 2019, Intel Corporation.
  * Copyright 2019 Google Inc.
  *
  * This program is free software; you can redistribute it and/or modify it
  * under the terms and conditions of the GNU General Public License,
  * version 2, as published by the Free Software Foundation.
  *
  * This program is distributed in the hope it will be useful, but WITHOUT
  * ANY WARRANTY; without evenp the implied warranty of MERCHANTABILITY or
  * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  * more details.
  */

#ifndef __DRIVERS_BUS_I2S_CAVS_1_8_REGS_H__
#define __DRIVERS_BUS_I2S_CAVS_1_8_REGS_H__

#define DSP_POWER_OFFSET	0x04
#define DSP_POWER_ON		0x000f0f0f
#define DSP_POWERED_UP		0x0f0f0f0f
#define AUDIO_DEV		0x1f
#define AUDIO_FUNC		3
#define POWER_OFFSET		0x8
#define BAR_OFFSET		0x804
#define ENABLE_ADSP_BAR		0x40000000
#define SERIAL_CLOCK_RATE	0x9

/* I2S1 register start address. */
#define	SSP_I2S1_START_ADDRESS	0x11000

#endif

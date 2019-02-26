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

#ifndef __DRIVERS_BUS_I2S_CAVS_1_5_REGS_H__
#define __DRIVERS_BUS_I2S_CAVS_1_5_REGS_H__

#define DSP_POWER_OFFSET	0x04
#define CLOCK_GATING_OFFSET	0x378
#define DSP_POWER_ON		0x00030303
#define DSP_POWERED_UP		0x03030303
#define DISABLE_CLOCK_GATING	0x0FFC0000
#define DISABLED_CLOCK_GATING	0x0DFC0000
#define AUDIO_DEV		0x0e
#define AUDIO_FUNC		0
#define POWER_OFFSET		0x8
#define BAR_OFFSET		0x804
#define ENABLE_ADSP_BAR		0x40000000
#define SERIAL_CLOCK_RATE	0x7

/* I2S1 and I2S5 register start address */
#define SSP_I2S1_START_ADDRESS	0x5000
#define SSP_I2S5_START_ADDRESS	0x3000

#endif

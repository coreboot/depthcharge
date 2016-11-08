/*
  * Copyright (c) 2016, Intel Corporation.
  * Copyright 2016 Google Inc.
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

#ifndef __DRIVERS_BUS_I2S_APOLLOLAKE_H__
#define __DRIVERS_BUS_I2S_APOLLOLAKE_H___

#include <pci.h>
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/apollolake.h"
#include "drivers/bus/i2s/i2s.h"

#define DSP_POWER_OFFSET	0x04
#define CLOCK_GATING_OFFSET	0x378
#define DSP_POWER_ON		0x00030303
#define DSP_POWERED_UP		0x03030303
#define DISABLE_CLOCK_GATING	0x0FFC0000
#define DISABLED_CLOCK_GATING	0x0DFC0000
#define RETRY_COUNT		1000
#define SSP_TIMEOUT		0
#define AUDIO_DEV		0x0e
#define POWER_OFFSET		0x8
#define BAR_OFFSET		0x804
#define ENABLE_ADSP_BAR		0x40000000

typedef enum {
	SSP_IN_NORMAL_MODE = 0x0,
	SSP_IN_NETWORK_MODE,
	SSP_INVALID_MODE = 0xF,
} AplSspMode;

typedef enum {
	TXD_TRISTATE_LAST_PHASE_OFF =  0x0,
	TXD_TRISTATE_LAST_PHASE_ON,
} AplSspTxdTristateLastPhase;

typedef enum {
	TXD_TRISTATE_OFF =  0x0,
	TXD_TRISTATE_ON,
} AplSspTxdTristateEnable;

typedef enum {
	NEXT_FRMS_ASS_AFTER_END_OF_T4 =  0x0,
	NEXT_FRMS_ASS_WITH_LSB_PREVIOUS_FRM,
} AplSspFrameSyncRelativeTimingBit;

typedef enum {
	DIV_DISABLE = 0x0,
	DIV_ENABLE,
} AplSspMnDividerEnable;


typedef enum {
	SSP_TS_1 = 0,
	SSP_TS_2,
	SSP_TS_4,
	SSP_TS_8,
	SSP_TS_SIZE
} AplSspTimeslot;

typedef enum {
	EDSS_4_16_BITS = 0,
	EDSS_17_32_BITS,
} AplDataSizeSelect;

typedef enum {
	NETWORK_CLOCK_DISABLE = 0,
	NETWORK_CLOCK_ENABLE,
} AplNetworkClockSelect;

typedef enum {
	SSP_DISABLE_CLOCK = 0,
	SSP_ENABLE_CLOCK,
} AplForceSSPClockRunning;

typedef enum {
	SSP_DMA_FINISH_ENABLE = 0,
	SSP_DMA_FINISH_DISABLE,
} AplSSPDMAFinishDisable;

typedef enum {
	TRANSMIT_UNDERRUN_MODE_1_DISABLE = 0,
	TRANSMIT_UNDERRUN_MODE_1_ENABLE,
} AplTransmitUnderrunMode1;

typedef enum {
	FRAME_RATE_CONTROL_STEREO = 2,
	FRAME_RATE_CONTROL_TDM8 = 8,
} FrameRateDividerControl;

/* Configuration-dependent settings. */
typedef struct AplI2sSettings {
	AplSspMode mode;
	uint8_t frame_rate_divider_ctrl;
	uint8_t ssp_psp_T4;
	uint8_t ssp_psp_T6;
	uint8_t ssp_active_tx_slots_map;
	uint8_t ssp_active_rx_slots_map;
	uint32_t m_value;
	uint32_t n_value;
} AplI2sSettings;

typedef struct {
	I2sOps ops;
	uintptr_t lpe_bar0;
	uintptr_t lpe_bar4;
	struct AplI2sRegs *regs;
	struct AplI2sRegs *shim;
	const struct AplI2sSettings *settings;

	int initialized;
	int bits_per_sample;
	GpioOps *sdmode_gpio;
} AplI2s;


AplI2s *new_apl_i2s(const AplI2sSettings *settings, int bps, GpioOps *sdmode);

#endif

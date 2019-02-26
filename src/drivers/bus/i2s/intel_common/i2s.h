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

#ifndef __DRIVERS_BUS_I2S_INTEL_COMMON_I2S_H__
#define __DRIVERS_BUS_I2S_INTEL_COMMON_I2S_H__

#include "drivers/bus/i2s/i2s.h"
#include "drivers/gpio/gpio.h"

#define RETRY_COUNT             1000
#define SSP_TIMEOUT             0
#define TX_FIFO_NOT_FULL        0x4

/* FIFO over/under-run interrupt config. */
enum {
        SSP_FIFO_INT_ENABLE = 0x0,
        SSP_FIFO_INT_DISABLE,
};

/* Frame format. */
enum {
        MOTOROLA_SPI_FORMAT = 0x0,
        TI_SSP_FORMAT,
        MICROWIRE_FORMAT,
        PSP_FORMAT,
};

typedef enum {
	SSP_IN_NORMAL_MODE,
	SSP_IN_NETWORK_MODE,
	SSP_INVALID_MODE = 0xF,
} SspMode;

typedef enum {
	TXD_TRISTATE_LAST_PHASE_OFF,
	TXD_TRISTATE_LAST_PHASE_ON,
} SspTxdTristateLastPhase;

typedef enum {
	TXD_TRISTATE_OFF,
	TXD_TRISTATE_ON,
} SspTxdTristateEnable;

typedef enum {
	NEXT_FRMS_ASS_AFTER_END_OF_T4,
	NEXT_FRMS_ASS_WITH_LSB_PREVIOUS_FRM,
} SspFrameSyncRelativeTimingBit;

typedef enum {
	DIV_DISABLE,
	DIV_ENABLE,
} SspMnDividerEnable;

typedef enum {
	SSP_TS_1,
	SSP_TS_2,
	SSP_TS_4,
	SSP_TS_8,
	SSP_TS_SIZE
} SspTimeslot;

typedef enum {
	EDSS_4_16_BITS,
	EDSS_17_32_BITS,
} DataSizeSelect;

typedef enum {
	NETWORK_CLOCK_DISABLE,
	NETWORK_CLOCK_ENABLE,
} NetworkClockSelect;

typedef enum {
	SSP_DISABLE_CLOCK,
	SSP_ENABLE_CLOCK,
} ForceSSPClockRunning;

typedef enum {
	SSP_DMA_FINISH_ENABLE,
	SSP_DMA_FINISH_DISABLE,
} SSPDMAFinishDisable;

typedef enum {
	TRANSMIT_UNDERRUN_MODE_1_DISABLE,
	TRANSMIT_UNDERRUN_MODE_1_ENABLE,
} TransmitUnderrunMode1;

typedef enum {
	FRAME_RATE_CONTROL_STEREO = 2,
	FRAME_RATE_CONTROL_TDM8 = 8,
} FrameRateDividerControl;

/* Configuration-dependent settings. */
typedef struct I2sSettings {
	/* SSP mode select bit defined network/normal mode.*/
	SspMode mode;
	/* Frame Rate Divider Control for SSPC0.*/
	uint8_t frame_rate_divider_ctrl;
	/* To set EDMYSTOP bit in SSPSP.*/
	uint8_t ssp_psp_T4;
	/* To set SFRMWDTH bit in SSPSP.*/
	uint8_t ssp_psp_T6;
	/* To set TTSA bit n SSTSA - data transmit timeslot. */
	uint8_t ssp_active_tx_slots_map;
	/* To set RTSA bit n SSRSA - data receive timeslot. */
	uint8_t ssp_active_rx_slots_map;
} I2sSettings;

typedef struct {
	/* I2S ops structure.*/
	I2sOps ops;
	/* Intel HD Audio Lower Base Address.*/
	uintptr_t lpe_bar0;
	/* Audio DSP Lower Base Address.*/
	uintptr_t lpe_bar4;
	/* I2S registers.*/
	struct I2sRegs *regs;
	/* Configuration-dependent settings. */
	const struct I2sSettings *settings;
	/* This bit is used to check if the bus is initialized or not */
	int initialized;
	/* Number of bits per sample */
	int bits_per_sample;
	/* Gpio ops structure- to set or clear gpio */
	GpioOps *sdmode_gpio;
} I2s;

/*
 * new_i2s_structure - Allocate new I2s data structures.
 * @settings: Codec settigns
 * @bps: Bits per sample
 */
I2s *new_i2s_structure(const I2sSettings *settings, int bps, GpioOps *sdmode,
	uintptr_t ssp_i2s_start_address);

#endif

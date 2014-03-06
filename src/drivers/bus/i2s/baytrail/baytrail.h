/*
  * Copyright (c) 2010, Intel Corporation.
  * Copyright 2014 Google Inc.
  *
  * This program is free software; you can redistribute it and/or modify it
  * under the terms and conditions of the GNU General Public License,
  * version 2, as published by the Free Software Foundation.
  *
  * This program is distributed in the hope it will be useful, but WITHOUT
  * ANY WARRANTY; without evenp the implied warranty of MERCHANTABILITY or
  * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  * more details.
  *
  * You should have received a copy of the GNU General Public License along with
  * this program; if not, write to the Free Software Foundation, Inc.,
  * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  */

#ifndef __DRIVERS_BUS_I2S_BAYTRAIL_BAYTRAIL_H__
#define __DRIVERS_BUS_I2S_BAYTRAIL_BAYTRAIL_H__

#include <pci.h>

#include "drivers/bus/i2s/i2s.h"

/* Configuration-dependent setting enums. */
typedef enum {
	SSP_IN_NORMAL_MODE = 0x0,
	SSP_IN_NETWORK_MODE,
	SSP_INVALID_MODE = 0xF,
} BytSspMode;

typedef enum {
	SSP_ONCHIP_CLOCK = 0x0,
	SSP_NETWORK_CLOCK,
	SSP_EXTERNAL_CLOCK,
	SSP_ONCHIP_AUDIO_CLOCK,
	SSP_MASTER_CLOCK_UNDEFINED = 0xFF,
} BytSspMasterModeClockSelection;

typedef enum {
	SSP_FRM_UNDEFINED = 0,
	SSP_FRM_48K,
	SSP_FRM_44K,
	SSP_FRM_22K,
	SSP_FRM_16K,
	SSP_FRM_11K,
	SSP_FRM_8K,
	SSP_FRM_SIZE,
} BytSspFrmFreq;

typedef enum {
	TXD_TRISTATE_LAST_PHASE_OFF =  0x0,
	TXD_TRISTATE_LAST_PHASE_ON,
} BytSspTxdTristateLastPhase;

typedef enum {
	TXD_TRISTATE_OFF =  0x0,
	TXD_TRISTATE_ON,
} BytSspTxdTristateEnable;

typedef enum {
	SLAVE_SSPCLK_ON_ALWAYS =  0x0,
	SLAVE_SSPCLK_ON_DURING_TRANSFER_ONLY,
} BytSspSlaveSspclkFreeRunning;

typedef enum {
	SSPSCLK_MASTER_MODE = 0x0,
	SSPSCLK_SLAVE_MODE,
} BytSspSspclkDirection;

typedef enum {
	SSPSFRM_MASTER_MODE = 0x0,
	SSPSFRM_SLAVE_MODE,
} BytSspSspsfrmDirection;

typedef enum {
	RX_AND_TX_MODE = 0x0,
	RX_WITHOUT_TX_MODE,
} BytSspRxWithoutTx;

typedef enum {
	SSP_TRAILING_BYTE_HDL_BY_IA = 0x0,
	SSP_TRAILING_BYTE_HDL_BY_DMA,
} BytTrailingByteMode;

typedef enum {
	SSP_TX_DMA_MASK = 0x0,
	SSP_TX_DMA_ENABLE,
} BytSspTxDmaStatus;

typedef enum {
	SSP_RX_DMA_MASK = 0x0,
	SSP_RX_DMA_ENABLE,
} BytSspRxDmaStatus;

typedef enum {
	SSP_RX_TIMEOUT_INT_DISABLE = 0x0,
	SSP_RX_TIMEOUT_INT_ENABLE,
} BytSspRxTimeoutIntStatus;

typedef enum {
	SSP_TRAILING_BYTE_INT_DISABLE = 0x0,
	SSP_TRAILING_BYTE_INT_ENABLE,
} BytSspTrailingByteIntStatus;

typedef enum {
	SSP_LOOPBACK_OFF = 0x0,
	SSP_LOOPBACK_ON,
} BytSspLoopbackModeStatus;

typedef enum {
	NEXT_FRMS_ASS_AFTER_END_OF_T4 =  0x0,
	NEXT_FRMS_ASS_WITH_LSB_PREVIOUS_FRM,
} BytSspFrameSyncRelativeTimingBit;

typedef enum {
	SSP_FRMS_ACTIVE_LOW =  0x0,
	SSP_FRMS_ACTIVE_HIGH,
} BytSspFrameSyncPolarityBit;

typedef enum {
	SSP_END_DATA_TRANSFER_STATE_LOW = 0x0,
	SSP_END_DATA_TRANSFER_STATE_PEVIOUS_BIT,
} BytSspEndOfTransferDataState;

typedef enum {
	SSP_CLK_MODE_0 = 0x0,
	SSP_CLK_MODE_1,
	SSP_CLK_MODE_2,
	SSP_CLK_MODE_3,
} BytSspClkMode;

typedef enum {
	BYPASS = 0x0,
	NO_BYPASS,
} BytSspMnDivider;

typedef enum {
	DIV_DISABLE = 0x0,
	DIV_ENABLE,
} BytSspMnDividerEnable;

typedef enum {
	MN_N0_UPDATE = 0x0,
	MN_UPDATE,
} BytSspMnDividerUpdate;

typedef enum {
	SSP_BPS_8 = 0,
	SSP_BPS_16,
	SSP_BPS_32,
	SSP_BPS_SIZE
} BytSspBitsPerSample;

typedef enum {
	SSP_TS_1 = 0,
	SSP_TS_2,
	SSP_TS_4,
	SSP_TS_8,
	SSP_TS_SIZE
} BytSspTimeslot;

typedef enum {
	FRAME_RATE_CONTROL_STEREO = 2,
	FRAME_RATE_CONTROL_TDM8 = 8,
} FrameRateDividerControl;

/* Configuration-dependent settings. */
typedef struct BytI2sSettings {
	BytSspMode mode;
	BytSspMasterModeClockSelection master_mode_clk_selection;
	uint8_t frame_rate_divider_control;
	BytSspFrmFreq master_mode_standard_freq;
	BytSspSspclkDirection sspslclk_direction;
	BytSspSspsfrmDirection sspsfrm_direction;
	BytTrailingByteMode ssp_trailing_byte_mode;
	BytSspRxTimeoutIntStatus ssp_rx_timeout_interrupt_status;
	BytSspLoopbackModeStatus ssp_loopback_mode_status;
	uint8_t ssp_rx_fifo_threshold;
	uint8_t ssp_tx_fifo_threshold;
	BytSspFrameSyncPolarityBit ssp_frmsync_pol_bit;
	BytSspEndOfTransferDataState ssp_end_transfer_state;
	BytSspClkMode ssp_serial_clk_mode;
	uint8_t ssp_psp_T1;
	uint8_t ssp_psp_T2;
	uint8_t ssp_psp_T4;
	uint8_t ssp_psp_T5;
	uint8_t ssp_psp_T6;
	uint8_t ssp_active_tx_slots_map;
	uint8_t ssp_active_rx_slots_map;
	BytSspMnDivider ssp_divider_bypass;
	BytSspMnDividerEnable ssp_divider_enable;
	BytSspMnDividerUpdate ssp_divider_update;
	uint32_t m_value;
	uint32_t n_value;
} BytI2sSettings;

typedef struct {
	I2sOps ops;

	struct BytI2sRegs *regs;
	struct BytI2sRegs *shim;
	const struct BytI2sSettings *settings;

	int initialized;
	int bits_per_sample;
	int channels;

	uint32_t clock_freq;
	uint32_t sampling_rate;
} BytI2s;


BytI2s *new_byt_i2s(uintptr_t mmio, const BytI2sSettings *settings,
		    int bps, int channels, uint32_t clock_freq,
		    uint32_t sampling_rate);

#endif

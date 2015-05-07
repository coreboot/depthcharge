/*
  * Copyright (c) 2015, Intel Corporation.
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

#ifndef __DRIVERS_BUS_I2S_BRASWELL_BRASWELL_H__
#define __DRIVERS_BUS_I2S_BRASWELL_BRASWELL_H___

#include <pci.h>

#include "drivers/bus/i2s/i2s.h"

/* Configuration-dependent setting enums. */
typedef enum {
	SSP_IN_NORMAL_MODE = 0x0,
	SSP_IN_NETWORK_MODE,
	SSP_INVALID_MODE = 0xF,
} BswSspMode;

typedef enum {
	SSP_ONCHIP_CLOCK = 0x0,
	SSP_NETWORK_CLOCK,
	SSP_EXTERNAL_CLOCK,
	SSP_ONCHIP_AUDIO_CLOCK,
	SSP_MASTER_CLOCK_UNDEFINED = 0xFF,
} BswSspMasterModeClockSelection;

typedef enum {
	SSP_FRM_UNDEFINED = 0,
	SSP_FRM_48K,
	SSP_FRM_44K,
	SSP_FRM_22K,
	SSP_FRM_16K,
	SSP_FRM_11K,
	SSP_FRM_8K,
	SSP_FRM_SIZE,
} BswSspFrmFreq;

typedef enum {
	TXD_TRISTATE_LAST_PHASE_OFF =  0x0,
	TXD_TRISTATE_LAST_PHASE_ON,
} BswSspTxdTristateLastPhase;

typedef enum {
	TXD_TRISTATE_OFF =  0x0,
	TXD_TRISTATE_ON,
} BswSspTxdTristateEnable;

typedef enum {
	SLAVE_SSPCLK_ON_ALWAYS =  0x0,
	SLAVE_SSPCLK_ON_DURING_TRANSFER_ONLY,
} BswSspSlaveSspclkFreeRunning;

typedef enum {
	SSPSCLK_MASTER_MODE = 0x0,
	SSPSCLK_SLAVE_MODE,
} BswSspSspclkDirection;

typedef enum {
	SSPSFRM_MASTER_MODE = 0x0,
	SSPSFRM_SLAVE_MODE,
} BswSspSspsfrmDirection;

typedef enum {
	RX_AND_TX_MODE = 0x0,
	RX_WITHOUT_TX_MODE,
} BswSspRxWithoutTx;

typedef enum {
	SSP_TRAILING_BYTE_HDL_BY_IA = 0x0,
	SSP_TRAILING_BYTE_HDL_BY_DMA,
} BswTrailingByteMode;

typedef enum {
	SSP_TX_DMA_MASK = 0x0,
	SSP_TX_DMA_ENABLE,
} BswSspTxDmaStatus;

typedef enum {
	SSP_RX_DMA_MASK = 0x0,
	SSP_RX_DMA_ENABLE,
} BswSspRxDmaStatus;

typedef enum {
	SSP_RX_TIMEOUT_INT_DISABLE = 0x0,
	SSP_RX_TIMEOUT_INT_ENABLE,
} BswSspRxTimeoutIntStatus;

typedef enum {
	SSP_TRAILING_BYTE_INT_DISABLE = 0x0,
	SSP_TRAILING_BYTE_INT_ENABLE,
} BswSspTrailingByteIntStatus;

typedef enum {
	SSP_LOOPBACK_OFF = 0x0,
	SSP_LOOPBACK_ON,
} BswSspLoopbackModeStatus;

typedef enum {
	NEXT_FRMS_ASS_AFTER_END_OF_T4 =  0x0,
	NEXT_FRMS_ASS_WITH_LSB_PREVIOUS_FRM,
} BswSspFrameSyncRelativeTimingBit;

typedef enum {
	SSP_FRMS_ACTIVE_LOW =  0x0,
	SSP_FRMS_ACTIVE_HIGH,
} BswSspFrameSyncPolarityBit;

typedef enum {
	SSP_END_DATA_TRANSFER_STATE_LOW = 0x0,
	SSP_END_DATA_TRANSFER_STATE_PEVIOUS_BIT,
} BswSspEndOfTransferDataState;

typedef enum {
	SSP_CLK_MODE_0 = 0x0,
	SSP_CLK_MODE_1,
	SSP_CLK_MODE_2,
	SSP_CLK_MODE_3,
} BswSspClkMode;

typedef enum {
	BYPASS = 0x0,
	NO_BYPASS,
} BswSspMnDivider;

typedef enum {
	DIV_DISABLE = 0x0,
	DIV_ENABLE,
} BswSspMnDividerEnable;

typedef enum {
	MN_N0_UPDATE = 0x0,
	MN_UPDATE,
} BswSspMnDividerUpdate;

typedef enum {
	SSP_BPS_8 = 0,
	SSP_BPS_16,
	SSP_BPS_32,
	SSP_BPS_SIZE
} BswSspBitsPerSample;

typedef enum {
	SSP_TS_1 = 0,
	SSP_TS_2,
	SSP_TS_4,
	SSP_TS_8,
	SSP_TS_SIZE
} BswSspTimeslot;

typedef enum {
	FRAME_RATE_CONTROL_STEREO = 2,
	FRAME_RATE_CONTROL_TDM8 = 8,
} FrameRateDividerControl;

/* Configuration-dependent settings. */
typedef struct BswI2sSettings {
	BswSspMode mode;
	BswSspMasterModeClockSelection master_mode_clk_selection;
	uint8_t frame_rate_divider_control;
	BswSspFrmFreq master_mode_standard_freq;
	BswSspSspclkDirection sspslclk_direction;
	BswSspSspsfrmDirection sspsfrm_direction;
	BswTrailingByteMode ssp_trailing_byte_mode;
	BswSspRxTimeoutIntStatus ssp_rx_timeout_interrupt_status;
	BswSspLoopbackModeStatus ssp_loopback_mode_status;
	uint8_t ssp_rx_fifo_threshold;
	uint8_t ssp_tx_fifo_threshold;
	BswSspFrameSyncPolarityBit ssp_frmsync_pol_bit;
	BswSspEndOfTransferDataState ssp_end_transfer_state;
	BswSspClkMode ssp_serial_clk_mode;
	uint8_t ssp_psp_T1;
	uint8_t ssp_psp_T2;
	uint8_t ssp_psp_T4;
	uint8_t ssp_psp_T5;
	uint8_t ssp_psp_T6;
	uint8_t ssp_active_tx_slots_map;
	uint8_t ssp_active_rx_slots_map;
	BswSspMnDivider ssp_divider_bypass;
	BswSspMnDividerEnable ssp_divider_enable;
	BswSspMnDividerUpdate ssp_divider_update;
	uint32_t m_value;
	uint32_t n_value;
} BswI2sSettings;

typedef struct {
	I2sOps ops;

	struct BswI2sRegs *regs;
	struct BswI2sRegs *shim;
	const struct BswI2sSettings *settings;

	int initialized;
	int bits_per_sample;
	int channels;

	uint32_t clock_freq;
	uint32_t sampling_rate;
} BswI2s;


BswI2s *new_bsw_i2s(uintptr_t mmio, const BswI2sSettings *settings,
		    int bps, int channels, uint32_t clock_freq,
		    uint32_t sampling_rate);

#endif

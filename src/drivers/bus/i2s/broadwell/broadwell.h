/*
 * Copyright (C) 2015 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_BUS_I2S_BROADWELL_BROADWELL_H__
#define __DRIVERS_BUS_I2S_BROADWELL_BROADWELL_H__

#include "drivers/bus/i2s/i2s.h"

typedef enum {
	BDW_SHIM_START_ADDRESS = 0xFB000,
	BDW_SSP0_START_ADDRESS = 0xFC000,
	BDW_SSP1_START_ADDRESS = 0xFD000,
} BdwSspBaseAddress;

enum {
	SSP_FIFO_SIZE = 7,
};

typedef enum {
	NEXT_FRMS_AFTER_END_OF_T4 =  0,
	NEXT_FRMS_WITH_LSB_PREVIOUS_FRM,
} BdwSspFrameSyncRelativeTiming;

typedef enum {
	SSP_FRMS_ACTIVE_LOW = 0,
	SSP_FRMS_ACTIVE_HIGH,
} BdwSspFrameSyncPolarity;

typedef enum {
	SSP_END_TRANSFER_STATE_LOW = 0,
	SSP_END_TRANSFER_STATE_PEVIOUS_BIT,
} BdwSspEndOfTransferDataState;

typedef enum {
	/* Data driven (falling), data sampled (rising), idle state (low) */
	SCLK_MODE_DDF_DSR_ISL,
	/* Data driven (rising), data sampled (falling), idle state (low) */
	SCLK_MODE_DDR_DSF_ISL,
	/* Data driven (rising), data sampled (falling), idle state (high) */
	SCLK_MODE_DDR_DSF_ISH,
	/* Data driven (falling), data sampled (rising), idle state (high) */
	SCLK_MODE_DDF_DSR_ISH,
} BdwSspSerialClockMode;

typedef struct BdwI2sSettings {
	BdwSspFrameSyncPolarity sfrm_polarity;
	BdwSspFrameSyncRelativeTiming sfrm_relative_timing;
	BdwSspEndOfTransferDataState end_transfer_state;
	BdwSspSerialClockMode sclk_mode;
	uint16_t sclk_rate;
	uint8_t sclk_dummy_stop;	/* 0-31 */
	uint8_t sclk_frame_width;	/* 1-38 */
} BdwI2sSettings;

typedef struct BdwI2sShimRegs {
	uint32_t csr;		/* 0x00 */
	uint32_t reserved0[29];	/* 0x14 - 0x77 */
	uint32_t clkctl;	/* 0x78 */
	uint32_t reserved1;	/* 0x7c */
	uint32_t cs2;		/* 0x80 */
} __attribute__ ((packed)) BdwI2sShimRegs;

typedef struct BdwI2sSspRegs {
	uint32_t sscr0;		/* 0x00 */
	uint32_t sscr1;		/* 0x04 */
	uint32_t sssr;		/* 0x08 */
	uint32_t ssitr;		/* 0x0c */
	uint32_t ssdr;		/* 0x10 */
	uint32_t reserved0[5];	/* 0x14 - 0x27 */
	uint32_t ssto;		/* 0x28 */
	uint32_t sspsp;		/* 0x2c */
	uint32_t sstsa;		/* 0x30 */
	uint32_t ssrsa;		/* 0x34 */
	uint32_t sstss;		/* 0x38 */
	uint32_t sscr2;		/* 0x40 */
	uint32_t sspsp2;	/* 0x44 */
} __attribute__ ((packed)) BdwI2sSspRegs;

typedef struct {
	I2sOps ops;

	BdwI2sShimRegs *shim;
	BdwI2sSspRegs *regs;

	const struct BdwI2sSettings *settings;
	int bits_per_sample;

	int initialized;
	int ssp;
} BdwI2s;

BdwI2s *new_bdw_i2s(uintptr_t base, int ssp, int bps,
	const BdwI2sSettings *settings);

#endif

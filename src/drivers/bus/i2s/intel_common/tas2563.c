/*
 * Copyright 2026 Google LLC
 * Copyright (c) 2026, Intel Corporation.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "drivers/bus/i2s/intel_common/tas2563.h"

/* I2S controller settings for TAS2563 codec. */
const I2sSettings tas2563_settings = {
	/* To set MOD bit in SSC0 - definig as network/normal mode */
	.mode = SSP_IN_NETWORK_MODE,
	/* To set FRDC bit in SSC0 - timeslot per frame in network mode */
	.frame_rate_divider_ctrl = FRAME_RATE_CONTROL_STEREO,
	.fsync_rate = 48000,
#if CONFIG(INTEL_COMMON_I2S_CAVS_1_5) || CONFIG(INTEL_COMMON_I2S_CAVS_1_8)
	.bclk_rate = 2400000,
#else
	.bclk_rate = 1536000,
#endif
	/* To set TTSA bit n SSTSA - data transmit timeslot */
	.ssp_active_tx_slots_map = 3,
	/* To set RTSA bit n SSRSA - data receive timeslot */
	.ssp_active_rx_slots_map = 3,
};

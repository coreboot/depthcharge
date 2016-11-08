/*
 * Copyright 2016 Google Inc.
 * Copyright (c) 2016, Intel Corporation.
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

#include "drivers/bus/i2s/apollolake/apollolake-max98357a.h"

/* Apollolake I2S controller settings for MAX98357a codec. */
const AplI2sSettings apollolake_max98357a_settings = {
	.mode = SSP_IN_NETWORK_MODE,
	/* To set MOD bit in SSC0 - definig as network/normal mode */
	.frame_rate_divider_ctrl = FRAME_RATE_CONTROL_STEREO,
	/* To set FRDC bit in SSC0 - timeslot per frame in network mode */
	.ssp_psp_T4 = 2,
	/* To set EDMYSTOP bit in SSPSP - number of SCLK cycles after data */
	.ssp_psp_T6 = 0x18,
	/* To set SFRMWDTH bit in SSPSP - frame width */
	.ssp_active_tx_slots_map = 3,
	/* To set TTSA bit n SSTSA - data transmit timeslot */
	.ssp_active_rx_slots_map = 3,
	/* To set RTSA bit n SSRSA - data receive timeslot */
	.m_value = 0x64,
	/* I2S Divider M value */
	.n_value = 0x18,
	/* I2S Divider N value */
};

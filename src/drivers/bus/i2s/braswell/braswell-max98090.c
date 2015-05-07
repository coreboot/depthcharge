/*
 * Copyright 2015 Google Inc.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include "drivers/bus/i2s/braswell/braswell-max98090.h"

/* Braswell I2S controller settings for MAX98090 codec. */
const BswI2sSettings braswell_max98090_settings = {
	.mode = SSP_IN_NETWORK_MODE,
	.master_mode_clk_selection = SSP_MASTER_CLOCK_UNDEFINED,
	.frame_rate_divider_control = FRAME_RATE_CONTROL_STEREO,
	.master_mode_standard_freq = 0xFFFF,
	.sspslclk_direction = SSPSCLK_MASTER_MODE,
	.sspsfrm_direction = SSPSFRM_MASTER_MODE,
	.ssp_trailing_byte_mode = SSP_TRAILING_BYTE_HDL_BY_IA,
	.ssp_rx_timeout_interrupt_status = SSP_RX_TIMEOUT_INT_DISABLE,
	.ssp_loopback_mode_status = SSP_LOOPBACK_OFF,
	.ssp_rx_fifo_threshold = 8,
	.ssp_tx_fifo_threshold = 7,
	.ssp_frmsync_pol_bit = SSP_FRMS_ACTIVE_LOW,
	.ssp_end_transfer_state = SSP_END_DATA_TRANSFER_STATE_LOW,
	.ssp_serial_clk_mode = SSP_CLK_MODE_0,
	.ssp_psp_T1 = 0,
	.ssp_psp_T2 = 1,
	.ssp_psp_T4 = 0,
	.ssp_psp_T5 = 0,
	.ssp_psp_T6 = 0x1F,
	.ssp_active_tx_slots_map = 3,
	.ssp_active_rx_slots_map = 3,
	.ssp_divider_bypass = BYPASS,
	.ssp_divider_enable = DIV_ENABLE,
	.ssp_divider_update = MN_UPDATE,
	.m_value = 0x60000180,
	.n_value = 0x00000C35,
};

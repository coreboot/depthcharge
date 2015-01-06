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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "drivers/bus/i2s/broadwell/broadwell-alc5677.h"

/* Broadwell I2S controller settings for ALC5677 codec. */
const BdwI2sSettings broadwell_alc5677_settings = {
	.sfrm_polarity = SSP_FRMS_ACTIVE_LOW,
	.end_transfer_state = SSP_END_TRANSFER_STATE_LOW,
	.sclk_mode = SCLK_MODE_DDF_DSR_ISL,
	.sfrm_relative_timing = NEXT_FRMS_WITH_LSB_PREVIOUS_FRM,
	.sclk_dummy_stop = 0,
	.sclk_frame_width = 31,
	.sclk_rate = 15,
};

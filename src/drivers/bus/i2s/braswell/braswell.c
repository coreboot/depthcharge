/*
 * Copyright (C) 2015 Intel Corporation
 * Copyright 2015 Google Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/bus/i2s/i2s.h"
#include "drivers/bus/i2s/braswell/braswell.h"
#include "drivers/bus/i2s/braswell/braswell-regs.h"

/* ssacd LUT */
static const uint8_t
ssp_ssacd[SSP_FRM_SIZE][SSP_BPS_SIZE][SSP_TS_SIZE] = {
	[SSP_FRM_UNDEFINED][SSP_BPS_8][SSP_TS_1] = SSP_SSACD_NOT_AVAILABLE,
	[SSP_FRM_UNDEFINED][SSP_BPS_8][SSP_TS_2] = SSP_SSACD_NOT_AVAILABLE,
	[SSP_FRM_UNDEFINED][SSP_BPS_8][SSP_TS_4] = SSP_SSACD_NOT_AVAILABLE,
	[SSP_FRM_UNDEFINED][SSP_BPS_8][SSP_TS_8] = SSP_SSACD_NOT_AVAILABLE,

	[SSP_FRM_UNDEFINED][SSP_BPS_16][SSP_TS_1] = SSP_SSACD_NOT_AVAILABLE,
	[SSP_FRM_UNDEFINED][SSP_BPS_16][SSP_TS_2] = SSP_SSACD_NOT_AVAILABLE,
	[SSP_FRM_UNDEFINED][SSP_BPS_16][SSP_TS_4] = SSP_SSACD_NOT_AVAILABLE,
	[SSP_FRM_UNDEFINED][SSP_BPS_16][SSP_TS_8] = SSP_SSACD_NOT_AVAILABLE,

	[SSP_FRM_UNDEFINED][SSP_BPS_32][SSP_TS_1] = SSP_SSACD_NOT_AVAILABLE,
	[SSP_FRM_UNDEFINED][SSP_BPS_32][SSP_TS_2] = SSP_SSACD_NOT_AVAILABLE,
	[SSP_FRM_UNDEFINED][SSP_BPS_32][SSP_TS_4] = SSP_SSACD_NOT_AVAILABLE,
	[SSP_FRM_UNDEFINED][SSP_BPS_32][SSP_TS_8] = SSP_SSACD_NOT_AVAILABLE,

	[SSP_FRM_48K][SSP_BPS_8][SSP_TS_1] =
		SSP_PLL_FREQ_12_235 | SSP_SYSCLK_DIV_8,
	[SSP_FRM_48K][SSP_BPS_8][SSP_TS_2] =
		SSP_PLL_FREQ_12_235 | SSP_SYSCLK_DIV_4,
	[SSP_FRM_48K][SSP_BPS_8][SSP_TS_4] =
		SSP_PLL_FREQ_12_235 | SSP_SYSCLK_DIV_2,
	[SSP_FRM_48K][SSP_BPS_8][SSP_TS_8] =
		SSP_PLL_FREQ_12_235 | SSP_SYSCLK_DIV_1,

	[SSP_FRM_48K][SSP_BPS_16][SSP_TS_1] =
		SSP_PLL_FREQ_12_235 | SSP_SYSCLK_DIV_4,
	[SSP_FRM_48K][SSP_BPS_16][SSP_TS_2] =
		SSP_PLL_FREQ_12_235 | SSP_SYSCLK_DIV_2,
	[SSP_FRM_48K][SSP_BPS_16][SSP_TS_4] =
		SSP_PLL_FREQ_12_235 | SSP_SYSCLK_DIV_1,
	[SSP_FRM_48K][SSP_BPS_16][SSP_TS_8] =
		SSP_PLL_FREQ_12_235 | SSP_SYSCLK_DIV_2 | SSP_SYSCLK_DIV4_BYPASS,

	[SSP_FRM_48K][SSP_BPS_32][SSP_TS_1] =
		SSP_PLL_FREQ_12_235 | SSP_SYSCLK_DIV_2,
	[SSP_FRM_48K][SSP_BPS_32][SSP_TS_2] =
		SSP_PLL_FREQ_12_235 | SSP_SYSCLK_DIV_1,
	[SSP_FRM_48K][SSP_BPS_32][SSP_TS_4] =
		SSP_PLL_FREQ_12_235 | SSP_SYSCLK_DIV_2 | SSP_SYSCLK_DIV4_BYPASS,
	[SSP_FRM_48K][SSP_BPS_32][SSP_TS_8] =
		SSP_PLL_FREQ_12_235 | SSP_SYSCLK_DIV_1 | SSP_SYSCLK_DIV4_BYPASS,

	[SSP_FRM_44K][SSP_BPS_8][SSP_TS_1] =
		SSP_PLL_FREQ_11_345 | SSP_SYSCLK_DIV_8,
	[SSP_FRM_44K][SSP_BPS_8][SSP_TS_2] =
		SSP_PLL_FREQ_11_345 | SSP_SYSCLK_DIV_4,
	[SSP_FRM_44K][SSP_BPS_8][SSP_TS_4] =
		SSP_PLL_FREQ_11_345 | SSP_SYSCLK_DIV_2,
	[SSP_FRM_44K][SSP_BPS_8][SSP_TS_8] =
		SSP_PLL_FREQ_11_345 | SSP_SYSCLK_DIV_1,

	[SSP_FRM_44K][SSP_BPS_16][SSP_TS_1] =
		SSP_PLL_FREQ_11_345 | SSP_SYSCLK_DIV_4,
	[SSP_FRM_44K][SSP_BPS_16][SSP_TS_2] =
		SSP_PLL_FREQ_11_345 | SSP_SYSCLK_DIV_2,
	[SSP_FRM_44K][SSP_BPS_16][SSP_TS_4] =
		SSP_PLL_FREQ_11_345 | SSP_SYSCLK_DIV_1,
	[SSP_FRM_44K][SSP_BPS_16][SSP_TS_8] =
		SSP_PLL_FREQ_11_345 | SSP_SYSCLK_DIV_2 | SSP_SYSCLK_DIV4_BYPASS,

	[SSP_FRM_44K][SSP_BPS_32][SSP_TS_1] =
		SSP_PLL_FREQ_11_345 | SSP_SYSCLK_DIV_2,
	[SSP_FRM_44K][SSP_BPS_32][SSP_TS_2] =
		SSP_PLL_FREQ_11_345 | SSP_SYSCLK_DIV_1,
	[SSP_FRM_44K][SSP_BPS_32][SSP_TS_4] =
		SSP_PLL_FREQ_11_345 | SSP_SYSCLK_DIV_2 | SSP_SYSCLK_DIV4_BYPASS,
	[SSP_FRM_44K][SSP_BPS_32][SSP_TS_8] =
		SSP_PLL_FREQ_11_345 | SSP_SYSCLK_DIV_1 | SSP_SYSCLK_DIV4_BYPASS,

	[SSP_FRM_22K][SSP_BPS_8][SSP_TS_1] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_8,
	[SSP_FRM_22K][SSP_BPS_8][SSP_TS_2] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_4,
	[SSP_FRM_22K][SSP_BPS_8][SSP_TS_4] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_2,
	[SSP_FRM_22K][SSP_BPS_8][SSP_TS_8] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_1,

	[SSP_FRM_22K][SSP_BPS_16][SSP_TS_1] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_4,
	[SSP_FRM_22K][SSP_BPS_16][SSP_TS_2] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_2,
	[SSP_FRM_22K][SSP_BPS_16][SSP_TS_4] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_1,
	[SSP_FRM_22K][SSP_BPS_16][SSP_TS_8] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_2 | SSP_SYSCLK_DIV4_BYPASS,

	[SSP_FRM_22K][SSP_BPS_32][SSP_TS_1] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_2,
	[SSP_FRM_22K][SSP_BPS_32][SSP_TS_2] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_1,
	[SSP_FRM_22K][SSP_BPS_32][SSP_TS_4] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_2 | SSP_SYSCLK_DIV4_BYPASS,
	[SSP_FRM_22K][SSP_BPS_32][SSP_TS_8] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_1 | SSP_SYSCLK_DIV4_BYPASS,

	[SSP_FRM_16K][SSP_BPS_8][SSP_TS_1] = SSP_SSACD_NOT_AVAILABLE,
	[SSP_FRM_16K][SSP_BPS_8][SSP_TS_2] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_32,
	[SSP_FRM_16K][SSP_BPS_8][SSP_TS_4] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_16,
	[SSP_FRM_16K][SSP_BPS_8][SSP_TS_8] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_8,

	[SSP_FRM_16K][SSP_BPS_16][SSP_TS_1] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_32,
	[SSP_FRM_16K][SSP_BPS_16][SSP_TS_2] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_16,
	[SSP_FRM_16K][SSP_BPS_16][SSP_TS_4] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_8,
	[SSP_FRM_16K][SSP_BPS_16][SSP_TS_8] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_4,

	[SSP_FRM_16K][SSP_BPS_32][SSP_TS_1] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_16,
	[SSP_FRM_16K][SSP_BPS_32][SSP_TS_2] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_8,
	[SSP_FRM_16K][SSP_BPS_32][SSP_TS_4] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_4,
	[SSP_FRM_16K][SSP_BPS_32][SSP_TS_8] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_2,

	[SSP_FRM_11K][SSP_BPS_8][SSP_TS_1] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_16,
	[SSP_FRM_11K][SSP_BPS_8][SSP_TS_2] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_8,
	[SSP_FRM_11K][SSP_BPS_8][SSP_TS_4] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_4,
	[SSP_FRM_11K][SSP_BPS_8][SSP_TS_8] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_2,

	[SSP_FRM_11K][SSP_BPS_16][SSP_TS_1] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_8,
	[SSP_FRM_11K][SSP_BPS_16][SSP_TS_2] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_4,
	[SSP_FRM_11K][SSP_BPS_16][SSP_TS_4] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_2,
	[SSP_FRM_11K][SSP_BPS_16][SSP_TS_8] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_1,

	[SSP_FRM_11K][SSP_BPS_32][SSP_TS_1] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_4,
	[SSP_FRM_11K][SSP_BPS_32][SSP_TS_2] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_2,
	[SSP_FRM_11K][SSP_BPS_32][SSP_TS_4] =
		SSP_PLL_FREQ_05_622 | SSP_SYSCLK_DIV_1,
	[SSP_FRM_11K][SSP_BPS_32][SSP_TS_8] =
		SSP_PLL_FREQ_11_345 | SSP_SYSCLK_DIV_1,

	[SSP_FRM_8K][SSP_BPS_8][SSP_TS_1] = SSP_SSACD_NOT_AVAILABLE,
	[SSP_FRM_8K][SSP_BPS_8][SSP_TS_2] = SSP_SSACD_NOT_AVAILABLE,
	[SSP_FRM_8K][SSP_BPS_8][SSP_TS_4] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_32,
	[SSP_FRM_8K][SSP_BPS_8][SSP_TS_8] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_16,

	[SSP_FRM_8K][SSP_BPS_16][SSP_TS_1] = SSP_SSACD_NOT_AVAILABLE,
	[SSP_FRM_8K][SSP_BPS_16][SSP_TS_2] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_32,
	[SSP_FRM_8K][SSP_BPS_16][SSP_TS_4] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_16,
	[SSP_FRM_8K][SSP_BPS_16][SSP_TS_8] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_8,

	[SSP_FRM_8K][SSP_BPS_32][SSP_TS_1] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_32,
	[SSP_FRM_8K][SSP_BPS_32][SSP_TS_2] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_16,
	[SSP_FRM_8K][SSP_BPS_32][SSP_TS_4] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_8,
	[SSP_FRM_8K][SSP_BPS_32][SSP_TS_8] =
		SSP_PLL_FREQ_32_842 | SSP_SYSCLK_DIV_4,
};

enum {
	LPE_SSP_FIFO_SIZE = 16,
};

/*
 * i2s_enable -  enable SSP device
 * @regs: pointer to registers
 * @settings: config settings
 * @streamdir: RX/TX direction
 *
 * Writes SSP register to enable the device
 */
static void i2s_enable(BswI2sRegs *regs,
		       const struct BswI2sSettings *settings,
		       int streamdir)
{
	uint32_t status;
	uint32_t sssr;
	uint32_t saved_SSCR3 = 0;
	uint64_t start;
	int i;

	status = read_SSCR0(regs);
	if (status & 0x80) {
		sssr = read_SSSR(regs);
		if (GET_SSSR_val(sssr, ROR)) {
			if (settings->frame_rate_divider_control ==
			    FRAME_RATE_CONTROL_STEREO) {
				saved_SSCR3 = read_SSCR3(regs);
				clear_SSCR3_reg(regs, I2S_TX_EN);
				clear_SSCR3_reg(regs, I2S_RX_EN);
			}
			clear_SSCR0_reg(regs, SSE);
			start = timer_us(0);
			while (read_SSCR0(regs) & 0x80 &&
			       timer_us(start) < 10000);
			set_SSCR0_reg(regs, SSE);

			if (settings->frame_rate_divider_control ==
			    FRAME_RATE_CONTROL_STEREO) {
				if (SSPSCLK_SLAVE_MODE ==
					settings->sspslclk_direction) {
					start = timer_us(0);
					while (read_SSSR(regs) & 0x400000 &&
					       timer_us(start) < 10000);
				}
				write_SSCR3(saved_SSCR3, regs);
			}
		}
	} else {
		set_SSCR0_reg(regs, SSE);

		if (SSPSCLK_MASTER_MODE == settings->sspslclk_direction &&
		    SSP_DATA_RX == streamdir &&
		    settings->frame_rate_divider_control !=
		    FRAME_RATE_CONTROL_STEREO) {
			for (i = 0; i < 8; ++i)
				write_SSDR(0x1FBEEFF8, regs);
		} else if (SSPSCLK_SLAVE_MODE == settings->sspslclk_direction
			  && settings->frame_rate_divider_control ==
			  FRAME_RATE_CONTROL_STEREO)
			while (read_SSSR(regs) & 0x400000);
	}
}

/**
 * i2s_disable -  disable SSP device
 * @regs: pointer to registers
 *
 * Writes SSP register to disable the device
 */
static void i2s_disable(BswI2sRegs *regs)
{
	clear_SSCR3_reg(regs, I2S_TX_EN);
	clear_SSCR3_reg(regs, I2S_RX_EN);
	clear_SSCR0_reg(regs, SSE);
}

/*
 * calculate_sspsp_psp - Calculate sspsp register
 * @settings: config settings
 *
 * Calculate sspsp register.
 */
static uint32_t calculate_sspsp_psp(const BswI2sSettings *settings)
{
	uint32_t sspsp;
	sspsp = SSPSP_reg(FSRT, NEXT_FRMS_ASS_AFTER_END_OF_T4) |
		SSPSP_reg(ETDS, settings->ssp_end_transfer_state) |
		SSPSP_reg(SCMODE, settings->ssp_serial_clk_mode) |
		SSPSP_reg(DMYSTOP, settings->ssp_psp_T4) |
		SSPSP_reg(SFRMDLY, settings->ssp_psp_T5) |
		SSPSP_reg(SFRMWDTH, settings->ssp_psp_T6) |
		SSPSP_reg(SFRMP, settings->ssp_frmsync_pol_bit);
	return sspsp;
}

/*
 * calculate_ssacd - Calculate ssacd register
 * @settings: config settings
 * @bps: bits per sample
 * @frame_rate_divider_new: calculated frame rate divider
 *
 * Calculate ssacd register
 *
 */
static uint32_t calculate_ssacd(const BswI2sSettings *settings, int bps,
	uint8_t *frame_rate_divider_new)
{
	uint8_t calculated_ssacd;
	BswSspTimeslot timeslots = 0;
	BswSspBitsPerSample bps_idx;
	BswSspFrmFreq freq = settings->master_mode_standard_freq;
	static uint8_t frame_rate_divider[SSP_TS_SIZE] = {
		[SSP_TS_1] = 1,
		[SSP_TS_2] = 2,
		[SSP_TS_4] = 4,
		[SSP_TS_8] = 8
	};

	switch (settings->frame_rate_divider_control) {
	case FRAME_RATE_CONTROL_STEREO:
		timeslots = SSP_TS_2;
		break;
	case FRAME_RATE_CONTROL_TDM8:
		timeslots = SSP_TS_8;
		break;
	};

	if (bps == 8)
		bps_idx = SSP_BPS_8;
	else if (bps == 16)
		bps_idx = SSP_BPS_16;
	else if (bps == 32)
		bps_idx = SSP_BPS_32;
	else {
		printf("Master mode bit per sample=%d unsupported\n", bps);
		return 0xFFFF;
	}

	while (timeslots < SSP_TS_8 &&
	       ssp_ssacd[freq][bps_idx][timeslots] == SSP_SSACD_NOT_AVAILABLE)
		timeslots++;

	calculated_ssacd = ssp_ssacd[freq][bps_idx][timeslots];
	if (calculated_ssacd == SSP_SSACD_NOT_AVAILABLE) {
		printf("Master mode unexpected error\n");
		return 0xFFFF;
	}

	*frame_rate_divider_new = frame_rate_divider[timeslots];

	return calculated_ssacd;
}


/*
 * calculate_sscr0_psp: Calculate sscr0 register
 * @settings: config settings
 * @bps: bits per sample
 *
 * Calculate sscr0 register
 */
static uint32_t calculate_sscr0_psp(const BswI2sSettings *settings, int bps)
{
	uint32_t sscr0;

	if (bps > 16)
		sscr0 = SSCR0_reg(DSS, SSCR0_DataSize(bps - 16)) |
			SSCR0_reg(EDSS, 1);
	else
		sscr0 = SSCR0_reg(DSS, SSCR0_DataSize(bps)) |
			SSCR0_reg(EDSS, 0);

	sscr0 |= SSCR0_reg(MOD, settings->mode) |
		 SSCR0_reg(FRF, PSP_FORMAT) |
		 SSCR0_reg(RIM, SSP_FIFO_INT_DISABLE) |
		 SSCR0_reg(TIM, SSP_FIFO_INT_DISABLE);
	return sscr0;
}

/**
 * calculate_sscr1_psp - Calculate sscr1 register.
 * @settings: config settings
 *
 * Calculate sscr1 register.
 */
static uint32_t calculate_sscr1_psp(const BswI2sSettings *settings)
{
	uint32_t sscr1;
	BswSspSspsfrmDirection i2s_frmdir_fix;

	i2s_frmdir_fix = settings->sspsfrm_direction;

	if (settings->frame_rate_divider_control ==
	    FRAME_RATE_CONTROL_STEREO)
		i2s_frmdir_fix = SSPSFRM_SLAVE_MODE;

	sscr1 = SSCR1_reg(SFRMDIR, i2s_frmdir_fix) |
		SSCR1_reg(SCLKDIR, settings->sspslclk_direction) |
		SSCR1_reg(EBCEI, 1) |
		SSCR1_reg(TTELP, TXD_TRISTATE_LAST_PHASE_OFF) |
		SSCR1_reg(TTE, TXD_TRISTATE_OFF) |
		SSCR1_reg(TRAIL, settings->ssp_trailing_byte_mode) |
		SSCR1_reg(TINTE, settings->ssp_rx_timeout_interrupt_status) |
		SSCR1_reg(LBM, settings->ssp_loopback_mode_status) |
		SSCR1_reg(RWOT, RX_AND_TX_MODE) |
		SSCR1_reg(RFT, SSCR1_RxTresh(settings->ssp_rx_fifo_threshold)) |
		SSCR1_reg(TFT, SSCR1_TxTresh(settings->ssp_tx_fifo_threshold));
	return sscr1;
}

/*
 * calculate_sscr2 - Calculate sscr2 register.
 * @settings: config settings
 * @clk_frame_mode: clock frame mode
 *
 * Calculate sscr2 register.
 *
 *
 */
static uint32_t calculate_sscr2(const BswI2sSettings *settings,
	uint16_t clk_frm_mode)


{
	uint32_t sscr2 = 0;
	if (settings->frame_rate_divider_control ==
	    FRAME_RATE_CONTROL_TDM8) {
		sscr2 |= (1 << SSCR2_UNDERRUN_FIX_1_SHIFT);
		if (clk_frm_mode == SSP_IN_SLAVE_MODE)
			sscr2 |= (1 << SSCR2_SLV_EXT_CLK_RUN_EN_SHIFT);
	}

	return sscr2;
}

/*
 * calculate_sscr3 - Calculate sscr3 register
 * @settings: config settings
 * @clk_frm_mode: clock frame mode
 *
 * Calculate sscr3 register
 */
static uint32_t calculate_sscr3(const BswI2sSettings *settings,
	uint16_t clk_frm_mode)
{
	uint32_t sscr3 = 0;

	sscr3 = ((1 << SSCR3_STRETCH_RX_SHIFT) | (1 << SSCR3_STRETCH_TX_SHIFT));

	if (settings->frame_rate_divider_control ==
	    FRAME_RATE_CONTROL_STEREO) {
		sscr3 |= (1 << SSCR3_I2S_RX_SS_FIX_EN_SHIFT) |
			 (1 << SSCR3_I2S_TX_SS_FIX_EN_SHIFT) |
			 (1 << SSCR3_I2S_MODE_EN_SHIFT) | 0x20004;
		if (clk_frm_mode == SSP_IN_MASTER_MODE) {
			sscr3 |= (1 << SSCR3_MST_CLK_EN_SHIFT) |
				 (1 << SSCR3_FRM_MST_EN_SHIFT);
		}

	}

	return sscr3;
}

/*
 * set_ssp_i2s_hw - Configure SSP driver according to settings.
 * @regs: pointer to registers
 * @shim: pointer to shim registers
 * @settings: config settings
 * @bps: bits per sample
 *
 * Configure SSP driver according to settings.
 */
static void set_ssp_i2s_hw(BswI2sRegs *regs, BswI2sRegs *shim,
	const BswI2sSettings *settings, int bps)
{
	uint32_t sscr0 = 0;
	uint32_t sscr1 = 0;
	uint32_t sscr2 = 0;
	uint32_t sscr3 = 0;
	uint32_t sscr4 = 0;
	uint32_t sscr5 = 0;
	uint32_t sstsa = 0;
	uint32_t ssrsa = 0;
	uint32_t sspsp = 0;
	uint32_t sssr = 0;
	uint32_t ssacd = 0;

	uint32_t ssp_divc_l = 0;
	uint32_t ssp_divc_h = 0;

	uint8_t frame_rate_divider;

	uint16_t clk_frm_mode = 0xFF;

	if (settings->sspsfrm_direction == SSPSFRM_MASTER_MODE &&
	    settings->sspslclk_direction == SSPSCLK_MASTER_MODE) {
		clk_frm_mode = SSP_IN_MASTER_MODE;
	} else if (settings->sspsfrm_direction == SSPSFRM_SLAVE_MODE &&
		 settings->sspslclk_direction == SSPSCLK_SLAVE_MODE) {
		clk_frm_mode = SSP_IN_SLAVE_MODE;
	} else {
		printf("Unsupported I2S PCM1 configuration\n");
		return;
	}
	frame_rate_divider = settings->frame_rate_divider_control;

	sscr0 = calculate_sscr0_psp(settings, bps);
	sscr1 = calculate_sscr1_psp(settings);

	if (settings->mode == SSP_IN_NETWORK_MODE)
		sscr0 |= SSCR0_reg(FRDC, SSCR0_SlotsPerFrm(frame_rate_divider));
	else if (settings->mode != SSP_IN_NORMAL_MODE) {
		printf("Unsupported mode\n");
		return;
	}

	sspsp = calculate_sspsp_psp(settings);
	sstsa = SSTSA_reg(TTSA, settings->ssp_active_tx_slots_map);
	ssrsa = SSRSA_reg(RTSA, settings->ssp_active_rx_slots_map);

	if (clk_frm_mode == SSP_IN_MASTER_MODE) {
		switch (settings->master_mode_clk_selection) {
		case SSP_MASTER_CLOCK_UNDEFINED:
		case SSP_ONCHIP_CLOCK:
			/* Use M/N divider directly to generate clock */
			break;
		case SSP_NETWORK_CLOCK:
			sscr0 |= SSCR0_reg(NCS, 1);
			break;
		case SSP_EXTERNAL_CLOCK:
			sscr0 |= SSCR0_reg(ECS, 1);
			break;
		case SSP_ONCHIP_AUDIO_CLOCK:
			ssacd = calculate_ssacd( settings, bps,
				&frame_rate_divider);
			if (ssacd == 0xFFFF) {
				printf("Error during SSACD calculation\n");
				return;
			}
			sscr0 |= SSCR0_reg(ACS, 1);
			break;
		}

		if (frame_rate_divider == FRAME_RATE_CONTROL_STEREO) {
			sscr5 = settings->ssp_psp_T6;
			sscr4 = (sscr5 + 1) *  frame_rate_divider;
			sscr4 = SSCR4_reg(TOT_FRM_PRD, sscr4);
			sscr5 = SSCR5_reg(FRM_ASRT_WIDTH, sscr5);
		}

		sspsp |= SSPSP_reg(STRTDLY, settings->ssp_psp_T1) |
			 SSPSP_reg(DMYSTRT, settings->ssp_psp_T2);

	} else
		sscr1 |= SSCR1_reg(SCFR, SLAVE_SSPCLK_ON_ALWAYS);

	if (frame_rate_divider == FRAME_RATE_CONTROL_STEREO) {
		sspsp &= ~SSPSP_reg(SFRMWDTH, -1);
		sspsp |= SSPSP_reg(SFRMWDTH, 0x1) |
			 SSPSP_reg(STRTDLY, settings->ssp_psp_T1) |
			 SSPSP_reg(DMYSTRT, settings->ssp_psp_T2);
	}

	ssp_divc_h |= LPE_SSP0_DIVC_H_reg(DIV_BYPASS,
					  settings->ssp_divider_bypass) |
		      LPE_SSP0_DIVC_H_reg(DIV_EN,
					  settings->ssp_divider_enable) |
		      LPE_SSP0_DIVC_H_reg(DIV_UPDATE,
					  settings->ssp_divider_update) |
		      LPE_SSP0_DIVC_H_reg(DIV_M, settings->m_value);

	ssp_divc_l |= LPE_SSP0_DIVC_L_reg(DIV_N, settings->n_value);
	sscr2 |= calculate_sscr2(settings, clk_frm_mode);
	sscr3 |= calculate_sscr3(settings, clk_frm_mode);

	sssr = (SSSR_BCE_MASK << SSSR_BCE_SHIFT) |
	       (SSSR_TUR_MASK << SSSR_TUR_SHIFT) |
	       (SSSR_TINT_MASK << SSSR_TINT_SHIFT) |
	       (SSSR_PINT_MASK << SSSR_PINT_SHIFT) |
	       (SSSR_ROR_MASK << SSSR_ROR_SHIFT);

	i2s_disable(regs);

	/* first set CR1 without interrupt and service enables */
	write_SSCR1(sscr1, regs);
	write_SSCR2(sscr2, regs);
	write_SSCR3(sscr3, regs);
	if (frame_rate_divider == FRAME_RATE_CONTROL_STEREO &&
	    clk_frm_mode == SSP_IN_MASTER_MODE) {
		write_SSCR4(sscr4, regs);
		write_SSCR5(sscr5, regs);
	}

	write_SSPSP(sspsp, regs);
	write_SSTSA(sstsa, regs);
	write_SSRSA(ssrsa, regs);

	/* Clear status */
	write_SSSR(sssr, regs);
	write_SSCR0(sscr0, regs);

	/* set the time out for the reception */
	write_SSTO(0, regs);

	write_LPE_SSP2_DIVC_L(ssp_divc_l, shim);
	write_LPE_SSP2_DIVC_H(ssp_divc_h, shim);
}

/*
 * bsw_i2s_init - Initialize I2s.
 * @bus: i2s config structure
 *
 * Initialize I2s.
 */
static int bsw_i2s_init(BswI2s *bus)
{

	i2s_disable(bus->regs);
	set_ssp_i2s_hw(bus->regs,
		       bus->shim,
		       bus->settings,
		       bus->bits_per_sample);
	return 0;
}

/*
 * bsw_i2s_send - Send audio samples to I2s controller.
 * @me: I2sOps structure
 * @data: Audio samples
 * @length: Number of samples
 *
 * Send audio samples to I2s controller.
 */
static int bsw_i2s_send(I2sOps *me, unsigned int *data, unsigned int length)
{
	int i;
	uint64_t start;
	BswI2s *bus = container_of(me, BswI2s, ops);
	struct BswI2sRegs *i2s_reg = bus->regs;

	if (!bus->initialized) {
		if (bsw_i2s_init(bus))
			return -1;
		bus->initialized = 1;
	}

	if (length < LPE_SSP_FIFO_SIZE) {
		printf("%s : Invalid data size\n", __func__);
		return -1;
	}

	i2s_enable(bus->regs, bus->settings, SSP_DATA_TX);
	for (i = 0; i < LPE_SSP_FIFO_SIZE; i++)
		writel(*data++, &i2s_reg->ssdr);

	length -= LPE_SSP_FIFO_SIZE;
	set_SSCR3_reg(i2s_reg, I2S_TX_EN);

	while (length > 0) {
		start = timer_us(0);
		if (readl(&i2s_reg->sssr) & 0x4) {
			writel(*data++, &i2s_reg->ssdr);
			length--;
		} else {
			if (timer_us(start) > 100000) {
				i2s_disable(bus->regs);
				printf("I2S Transfer Timeout\n" );
				return -1;
			}
		}
	}

	i2s_disable(bus->regs);
	return 0;
}

/*
 * new_bsw_i2s - Allocate new I2s data structures.
 * @mmio: memory-mapped base address for controller
 * @mode: Implementation-specific driver mode
 * @bps: Bits per sample
 * @channels: Number of channels
 * @clock_frequenct: I2c frequency
 * @sampling_rate: Audio sample rate
 *
 * Allocate new I2s data structures.
 */
BswI2s *new_bsw_i2s(uintptr_t mmio, const BswI2sSettings *settings,
		    int bps, int channels, uint32_t clock_freq,
		    uint32_t sampling_rate)
{
	BswI2s *bus = xzalloc(sizeof(*bus));

	bus->ops.send = &bsw_i2s_send;
	bus->regs = (BswI2sRegs *)(mmio + BSW_SSP2_START_ADDRESS);
	bus->shim = (BswI2sRegs *)(mmio + BSW_SSP2_SHIM_START_ADDRESS);
	bus->settings = settings;
	bus->bits_per_sample = bps;
	bus->channels = channels;
	bus->clock_freq = clock_freq;
	bus->sampling_rate = sampling_rate;
	return bus;
}

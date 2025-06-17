// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2023 Google LLC.
 */

#include <libpayload.h>

#include "drivers/sound/max98363_sndw.h"

/* MAX98363 codec ID */
static const sndw_codec_id max98363_id = {
	.codec = {
		.version   = 0x31,
		.mfgid1    = 0x01,
		.mfgid2    = 0x9f,
		.partid1   = 0x83,
		.partid2   = 0x63,
		.sndwclass = 0x0,
	}
};

/*
 * Soundwire messages for programming the MAX98363 codec registers to generate
 * boot beep via Tone generator.
 */
static const sndw_cmd max98363_beep_init_cmds[] = {
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_INTERRUPT_CLEAR,
		.regdata = MAX98363_REG_INTERRUPT_CLEAR_ALL,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_SCP_INTMASK_1,
		.regdata = MAX98363_SCP_INTMASK_PARITY | MAX98363_SCP_INTMASK_BUS_CLASH,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_ERROR_MONITOR_CTRL,
		.regdata = MAX98363_MON_DISABLE_ALL,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_SPKR_MONITOR_THRESHOLD,
		.regdata = MAX98363_SPKMON_THRESHOLD_0,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_SPKR_MONITOR_DURATION,
		.regdata = MAX98363_SPKMON_DURATION_0,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_TONE_CONFIG,
		.regdata = MAX98363_TONE_CONFIG_F1000,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_AMP_VOLUME,
		.regdata = MAX98363_SPEAKER_AMP_VOLUME_M6DB,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_AMP_PATH_GAIN,
		.regdata = MAX98363_SPEAKER_AMP_GAIN_12DB,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_AMP_DSP_CONFIG,
		.regdata = MAX98363_SPK_VOL_RMPUP_EN,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_SCP_BUS_CLOCK_BASE,
		.regdata = MAX98363_SCP_BASE_CLOCK_19200K,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_CLOCK_SCALE_B0,
		.regdata = MAX98363_SCALING_FACTOR_4,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_CLOCK_SCALE_B1,
		.regdata = MAX98363_SCALING_FACTOR_4,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_DP1_BLOCK_CTRL,
		.regdata = MAX98363_WORD_LENGTH_24,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_SCP_FRAME_CTRL_B0,
		.regdata = (MAX98363_ROW_CONTROL_48 << MAX98363_ROW_CONTROL_SHIFT) |
			(MAX98363_COLUMN_CONTROL_02 << MAX98363_COLUMN_CONTROL_SHIFT),
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_DP1_SAMPLE_CTRL1_B0,
		.regdata = MAX98363_SAMPLE_INTERVAL_LOW_200,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_DP1_OFFSET_CTRL1_B0,
		.regdata = MAX98363_OFFSET1_1,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_DP1_H_CTRL_B0,
		.regdata = (MAX98363_H_START_3 << MAX98363_H_START_SHIFT) |
			(MAX98363_H_STOP_3 << MAX98363_H_STOP_SHIFT),
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_DP1_SAMPLE_CTRL1_B1,
		.regdata = MAX98363_SAMPLE_INTERVAL_LOW_200,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_DP1_OFFSET_CTRL1_B1,
		.regdata = MAX98363_OFFSET1_1,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_DP1_H_CTRL_B1,
		.regdata = (MAX98363_H_START_3 << MAX98363_H_START_SHIFT) |
			(MAX98363_H_STOP_3 << MAX98363_H_STOP_SHIFT),
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_DP1_CHANNEL_EN_B1,
		.regdata = MAX98363_DP1_CHANNEL_EN_CH1,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = MAX98363_REG_DP1_PREPARE_CTRL,
		.regdata = MAX98363_PREPARE_CH1,
	}
};

static const sndw_cmd max98363_status_cmd = {
	.cmdtype = cmd_read,
	.regaddr = MAX98363_REG_DP1_PREPARE_STATUS,
};

static const sndw_cmd max98363_beep_activate_dp1_cmd = {
	.cmdtype = cmd_write,
	.regaddr = MAX98363_REG_SCP_FRAME_CTRL_B1,
	.regdata = (MAX98363_ROW_CONTROL_48 << MAX98363_ROW_CONTROL_SHIFT) |
		(MAX98363_COLUMN_CONTROL_02 << MAX98363_COLUMN_CONTROL_SHIFT),
};

static const sndw_cmd max98363_beepon_cmd = {
	.cmdtype = cmd_write,
	.regaddr = MAX98363_REG_TONE_GENERATOR,
	.regdata = MAX98363_TONE_GENERATOR_ENABLE,
};

static const sndw_cmd max98363_beepoff_cmd = {
	.cmdtype = cmd_write,
	.regaddr = MAX98363_REG_TONE_GENERATOR,
	.regdata = MAX98363_TONE_GENERATOR_DISABLE,
};

/*
 * sndwbeep - Function program the registers to turn on the tone generator
 * in the codec.
 * sndw - Pointer to the Soundwire ops.
 * sndwlinkaddr - Soundwire controller link address.
 * sndwendpointdev - Endpoint device number for the codec.
 */
static int sndwbeep(SndwOps *sndw, void *sndwlinkaddr, uint32_t sndwendpointdev,
		    uint32_t beepduration)
{
	size_t i;

	/* Send init commands. */
	for (i = 0; i < ARRAY_SIZE(max98363_beep_init_cmds); i++) {
		if (sndw->sndw_sendwack(sndwlinkaddr, max98363_beep_init_cmds[i],
					sndwendpointdev, NULL)) {
			printf("Failed to send init commands. \n");
			return -1;
		}
	}

	/* Check the status of DP1 */
	if (sndw->sndw_sendwack(sndwlinkaddr, max98363_status_cmd,
				sndwendpointdev, NULL)) {
		printf("Failed to get status of DP1.\n");
		return -1;
	}

	/* Send the cmd to activate the DP1 */
	if (sndw->sndw_sendwack(sndwlinkaddr, max98363_beep_activate_dp1_cmd,
				sndwendpointdev, NULL)) {
		printf("Failed to send the DP1 activate command. \n");
		return -1;
	}

	/* Send the cmd to enable the tone generator */
	if (sndw->sndw_sendwack(sndwlinkaddr, max98363_beepon_cmd,
				sndwendpointdev, NULL)) {
		printf("Failed to send the beep ON command. \n");
		return -1;
	}

	/* Beep duration */
	mdelay(beepduration);

	/* Send the cmd to disable the tone generator */
	if (sndw->sndw_sendwack(sndwlinkaddr, max98363_beepoff_cmd,
				sndwendpointdev, NULL)) {
		printf("Failed to send the beep OFF command. \n");
		return -1;
	}

	return 0;
}

/*
 * max98363sndw_beep - Function involves enabling of the codec, generate the beep via
 * tone generator and then resets the DSP.
 */
static int max98363sndw_beep(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	Max98363sndw *codec = container_of(me, Max98363sndw, ops);
	sndw_codec_info maxcodecinfo;

	if (codec->sndw->sndw_enable(codec->sndw, &maxcodecinfo)) {
		printf("Failed to enable the soundwire codec.\n");
		return -1;
	}

	for(int i = 0; i < SNDW_DEV_ID_NUM; i++) {
		if(max98363_id.id[i] != maxcodecinfo.codecid.id[i]) {
			printf("Codecs don't match\n");
			for(int j = 0; j < SNDW_DEV_ID_NUM; j++)
				printf(" 0x%x", maxcodecinfo.codecid.id[j]);
			printf("\n");
			return -1;
		}
	}

	if (sndwbeep(codec->sndw, maxcodecinfo.sndwlinkaddr, maxcodecinfo.deviceindex,
							codec->beepduration)) {
		printf("Failed to generate the boot beep.\n");
		return -1;
	}

	if (codec->sndw->sndw_disable(codec->sndw)) {
		printf("Failed to reset the DSP.\n");
		return -1;
	}

	printf("Generated Soundwire Boot beep\n");
	return 0;
}

/*
 * new_max98363_sndw - new structure for Soundwire Max98363 codec.
 * sndw - Pointer to the Soundwire ops.
 */
Max98363sndw *new_max98363_sndw(SndwOps *sndw, uint32_t beepduration)
{
	Max98363sndw *codec = xzalloc(sizeof(*codec));

	codec->ops.play = &max98363sndw_beep;
	codec->sndw = sndw;
	codec->beepduration = beepduration;

	return codec;
}

// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Intel Corporation.
 * Copyright 2021 Google LLC.
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/sound/max98373_sndw.h"

/* MAX98373 codec ID */
static const sndw_codec_id max98373_id = {
	.codec = {
		.version   = 0x27,
		.mfgid1    = 0x01,
		.mfgid2    = 0x9f,
		.partid1   = 0x83,
		.partid2   = 0x73,
		.sndwclass = 0x0,
	}
};

/*
 * Soundwire messages for programming the MAX98373 codec registers to generate
 * boot beep via Tone generator.
 */
static const sndw_cmd max98373_beep_init_cmds[] = {
	{
		.cmdtype = cmd_write,
		.regaddr = REG_SOFT_RESET,
		.regdata = SOFT_RESET_TRIGGER,
	},
	{
		.cmdtype = cmd_write,
		.regaddr = REG_MODE_CONFIG,
		.regdata = SNDW_ENDPOINT_MODE,
	}
};

static const sndw_cmd max98373_beep_ampcontrol_cmd = {
	.cmdtype = cmd_write,
	.regaddr = REG_AMP_VOLUME,
	.regdata = SPEAKER_AMP_VOLUME_M32DB,
};

static const sndw_cmd max98373_beep_freqcontrol_cmd = {
	.cmdtype = cmd_write,
	.regaddr = REG_TONE_CONFIG,
	.regdata = TONE_CONFIG_FS_4,
};

static const sndw_cmd max98373_beep_speakeren_cmd = {
	.cmdtype = cmd_write,
	.regaddr = REG_SPEAKER_PATH,
	.regdata = SPEAKER_ENABLE,
};

static const sndw_cmd max98373_beepon_cmd = {
	.cmdtype = cmd_write,
	.regaddr = REG_GLOBAL_ENABLE,
	.regdata = GLOBAL_ENABLE,
};

static const sndw_cmd max98373_beepoff_cmd = {
	.cmdtype = cmd_write,
	.regaddr = REG_GLOBAL_ENABLE,
	.regdata = GLOBAL_DISABLE,
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

	/* Send init commands to perform the software reset and configure the mode. */
	for (i = 0; i < ARRAY_SIZE(max98373_beep_init_cmds); i++) {
		if (sndw->sndw_sendwack(sndwlinkaddr, max98373_beep_init_cmds[i],
									sndwendpointdev)) {
			printf("Failed to send the beep init command. \n");
			return -1;
		}
	}

	/* Send the cmd to set the tone generator configuration */
	if (sndw->sndw_sendwack(sndwlinkaddr, max98373_beep_freqcontrol_cmd, sndwendpointdev)) {
		printf("Failed to send the beep frequency control command. \n");
		return -1;
	}

	/* Send the cmd to set the speaker volume control */
	if (sndw->sndw_sendwack(sndwlinkaddr, max98373_beep_ampcontrol_cmd, sndwendpointdev)) {
		printf("Failed to send the beep amplitude control command. \n");
		return -1;
	}

	/* Send the cmd to enable the speaker Path */
	if (sndw->sndw_sendwack(sndwlinkaddr, max98373_beep_speakeren_cmd, sndwendpointdev)) {
		printf("Failed to send the speaker enable command. \n");
		return -1;
	}

	/* Send the cmd for global enable to turn on the beep */
	if (sndw->sndw_sendwack(sndwlinkaddr, max98373_beepon_cmd, sndwendpointdev)) {
		printf("Failed to send the beep ON command. \n");
		return -1;
	}

	/* Beep duration */
	mdelay(beepduration);

	/* Send the cmd for global enable to turn off the beep */
	if (sndw->sndw_sendwack(sndwlinkaddr, max98373_beepoff_cmd, sndwendpointdev)) {
		printf("Failed to send the beep OFF command. \n");
		return -1;
	}

	return 0;
}

/*
 * max98373sndw_beep - Function involves enabling of the codec, generate the beep via
 * tone generator and then resets the DSP.
 */
static int max98373sndw_beep(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	Max98373sndw *codec = container_of(me, Max98373sndw, ops);
	sndw_codec_info maxcodecinfo;

	if (codec->sndw->sndw_enable(codec->sndw, &maxcodecinfo)) {
		printf("Failed to enable the soundwire codec.\n");
		return -1;
	}

	for(int i = 0; i < SNDW_DEV_ID_NUM; i++) {
		if(max98373_id.id[i] != maxcodecinfo.codecid.id[i]) {
			printf("Codecs don't match\n");
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
 * new_max98373_sndw - new structure for Soundwire Max98373 codec.
 * sndw - Pointer to the Soundwire ops.
 */
Max98373sndw *new_max98373_sndw(SndwOps *sndw, uint32_t beepduration)
{
	Max98373sndw *codec = xzalloc(sizeof(*codec));

	codec->ops.play = &max98373sndw_beep;
	codec->sndw = sndw;
	codec->beepduration = beepduration;

	return codec;
}

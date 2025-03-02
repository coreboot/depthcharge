// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2025 Google LLC.
 */
#include <libpayload.h>
#include "drivers/sound/rt721_sndw.h"

/* rt721 codec ID */
static const sndw_codec_id rt721_id = {
	.codec = {
		.version   = 0x30,
		.mfgid1    = 0x02,
		.mfgid2    = 0x5d,
		.partid1   = 0x07,
		.partid2   = 0x21,
		.sndwclass = 0x1,
	}
};

/*
 * Soundwire messages for programming the ALC721 codec registers to generate
 * boot beep via Tone generator.
 */
static const sndw_cmd alc721_addr_page1_cmd = {
		.cmdtype = cmd_write,
		.regaddr = RT721_SCP_ADDRPAGE1,
		.regdata = RT721_SCP_ADDRPAGE1_FUNC4_CONFIG,
};

static const sndw_cmd alc721_addr_page2_cmd = {
		.cmdtype = cmd_write,
		.regaddr = RT721_SCP_ADDRPAGE2,
		.regdata = RT721_SCP_ADDRPAGE2_TONE_GENERATOR_CONTROL,
};

static const sndw_cmd alc721_beep_on_cmd = {
		.cmdtype = cmd_write,
		.regaddr = RT721_TONE_GENERATOR,
		.regdata = RT721_TONE_GENERATOR_ENABLE,
};

static const sndw_cmd alc721_beep_off_cmd = {
		.cmdtype = cmd_write,
		.regaddr = RT721_TONE_GENERATOR,
		.regdata = RT721_TONE_GENERATOR_DISABLE,
};

/*
 * sndw_beep - Function program the registers to turn on the tone generator
 * in the codec.
 * sndw - Pointer to the Soundwire ops.
 * sndwlinkaddr - Soundwire controller link address.
 * sndwendpointdev - Endpoint device number for the codec.
 */
static int sndw_beep(SndwOps *sndw, void *sndwlinkaddr, uint32_t sndwendpointdev,
		    uint32_t beep_duration)
{
	printf("%s start\n", __func__);

	/* Send the cmd to write to scp address page 1 register to access function 4*/
	if (sndw->sndw_sendwack(sndwlinkaddr, alc721_addr_page1_cmd, sndwendpointdev)) {
		printf("Failed to send the scp addr page1 command.\n");
		return -1;
	}
	/* Send the cmd to write to scp address page 2 register to access the tone generator */
	if (sndw->sndw_sendwack(sndwlinkaddr, alc721_addr_page2_cmd, sndwendpointdev)) {
		printf("Failed to send the scp addr page2 command.\n");
		return -1;
	}
	/* Send the cmd to enable the tone generator */
	if (sndw->sndw_sendwack(sndwlinkaddr, alc721_beep_on_cmd, sndwendpointdev)) {
		printf("Failed to send the beep ON command.\n");
		return -1;
	}
	/* Beep duration */
	mdelay(beep_duration);
	/* Send the cmd to disable the tone generator */
	if (sndw->sndw_sendwack(sndwlinkaddr, alc721_beep_off_cmd, sndwendpointdev)) {
		printf("Failed to send the beep OFF command.\n");
		return -1;
	}

	printf("%s end\n", __func__);
	return 0;
}
/*
 * rt721sndw_beep - Function involves enabling of the codec, generate the beep via
 * tone generator and then resets the DSP.
 */
static int rt721sndw_beep(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	rt721sndw *codec = container_of(me, rt721sndw, ops);
	sndw_codec_info maxcodecinfo;

	if (codec->sndw->sndw_enable(codec->sndw, &maxcodecinfo)) {
		printf("Failed to enable the soundwire codec.\n");
		return -1;
	}

	for(int i = 0; i < SNDW_DEV_ID_NUM; i++) {
		if(rt721_id.id[i] != maxcodecinfo.codecid.id[i]) {
			printf("Codecs don't match\n");
			for(int j = 0; j < SNDW_DEV_ID_NUM; j++)
				printf(" 0x%x", maxcodecinfo.codecid.id[j]);
			printf("\n");
			return -1;
		}
	}

	if (sndw_beep(codec->sndw, maxcodecinfo.sndwlinkaddr, maxcodecinfo.deviceindex,
							codec->beep_duration)) {
		printf("Failed to generate the boot beep.\n");
		return -1;
	}

	if (codec->sndw->sndw_disable(codec->sndw)) {
		printf("Failed to reset the DSP.\n");
		return -1;
	}

	printf("Generated rt721sndw boot beep\n");
	return 0;
}
/*
 * new_rt721_sndw - new structure for Soundwire rt721 codec.
 * sndw - Pointer to the Soundwire ops.
 */
rt721sndw *new_rt721_sndw(SndwOps *sndw, uint32_t beep_duration)
{
	rt721sndw *codec = xzalloc(sizeof(*codec));
	codec->ops.play = &rt721sndw_beep;
	codec->sndw = sndw;
	codec->beep_duration = beep_duration;

	return codec;
}

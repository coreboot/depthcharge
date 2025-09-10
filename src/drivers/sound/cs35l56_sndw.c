#include <libpayload.h>
#include "drivers/sound/cs35l56_sndw.h"
#define SNDW_WRITE(addr, data) \
	{ .cmdtype = cmd_write, .regaddr = addr, .regdata = data }

const sndw_codec_id cs35l56_id = {
	.codec = {
		.version   = 0x31,
		.mfgid1    = 0x01,
		.mfgid2    = 0xfa,
		.partid1   = 0x35,
		.partid2   = 0x56,
		.sndwclass = 0x01,
	}
};

static const sndw_cmd cs35l56_write_page1 = {
	.cmdtype = cmd_write,
	.regaddr = PCP_ADDR_PAGE1,
	.regdata = PCP_ADDR_PAGE1_CONFIG,
};

static const sndw_cmd cs35l56_write_page2 = {
	.cmdtype = cmd_write,
	.regaddr = PCP_ADDR_PAGE2,
	.regdata = PCP_ADDR_PAGE2_CONFIG,
};

static const sndw_cmd cs35l56_tone_enable[] = {
	SNDW_WRITE(REGADDR0, TONE_GENERATOR_ENABLE),
	SNDW_WRITE(REGADDR1, WRITE_NONE),
	SNDW_WRITE(REGADDR2, WRITE_NONE),
	SNDW_WRITE(REGADDR3, TONE_GENERATOR3),
};

static const sndw_cmd cs35l56_tone_disable[] = {
	SNDW_WRITE(REGADDR0, TONE_GENERATOR_DISABLE),
	SNDW_WRITE(REGADDR1, WRITE_NONE),
	SNDW_WRITE(REGADDR2, WRITE_NONE),
	SNDW_WRITE(REGADDR3, TONE_GENERATOR3),
};

static int sndw_beep(SndwOps *sndw, void *sndwlinkaddr, uint32_t sndwendpointdev,
		    uint32_t beep_duration)
{
	printf("%s start\n", __func__);

	/* Send the cmd to write to scp address page 1 */
	if (sndw->sndw_sendwack(sndwlinkaddr, cs35l56_write_page1,
				sndwendpointdev, NULL)) {
		printf("Failed to write PCP_ADDR_PAGE1.\n");
		return -1;
	}
	/* Send the cmd to write to scp address page 2 */
	if (sndw->sndw_sendwack(sndwlinkaddr, cs35l56_write_page2,
				sndwendpointdev, NULL)) {
		printf("Failed to write PCP_ADDR_PAGE2.\n");
		return -1;
	}
	/* Send the cmd to enable the tone generator */
	for (int i = 0; i < ARRAY_SIZE(cs35l56_tone_enable); i++) {
		if (sndw->sndw_sendwack(sndwlinkaddr, cs35l56_tone_enable[i],
					sndwendpointdev, NULL)) {
			printf("Failed to send cs35l56_tone_enable. \n");
			return -1;
		}
	}

	/* Beep duration */
	mdelay(beep_duration);
	/* Send the cmd to disable the tone generator */
	for (int i = 0; i < ARRAY_SIZE(cs35l56_tone_disable); i++) {
		if (sndw->sndw_sendwack(sndwlinkaddr, cs35l56_tone_disable[i],
					sndwendpointdev, NULL)) {
			printf("Failed to send cs35l56_tone_disable. \n");
			return -1;
		}
	}

	printf("%s end\n", __func__);
	return 0;
}

static int cs35l56sndw_beep(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	SoundDevice_sndw *codec = container_of(me, SoundDevice_sndw, ops);
	sndw_codec_info info;

	if (codec->sndw->sndw_enable(codec->sndw, &info)) {
		printf("Failed to enable codec.\n");
		return -1;
	}

	for (int i = 0; i < SNDW_DEV_ID_NUM; i++) {
		if (cs35l56_id.id[i] != info.codecid.id[i]) {
			printf("Codec ID mismatch.\n");
			for (int j = 0; j < SNDW_DEV_ID_NUM; j++)
				printf(" 0x%x", info.codecid.id[j]);
			printf("\n");
			return -1;
		}
	}

	if (sndw_beep(codec->sndw, info.sndwlinkaddr, info.deviceindex, msec))
		return -1;

	if (codec->sndw->sndw_disable(codec->sndw)) {
		printf("Failed to disable codec.\n");
		return -1;
	}

	return 0;
}

SoundDevice_sndw *new_cs35l56_sndw(SndwOps *sndw, uint32_t beep_duration)
{
	SoundDevice_sndw *codec = xzalloc(sizeof(*codec));
	codec->ops.play = &cs35l56sndw_beep;
	codec->sndw = sndw;
	codec->beep_duration = beep_duration;

	return codec;
}

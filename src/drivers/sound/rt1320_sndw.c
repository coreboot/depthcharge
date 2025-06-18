// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2025 Google LLC.
 */
#include <libpayload.h>
#include "drivers/sound/rt1320_sndw.h"

/* ALC1320 amplifier ID */
static const sndw_codec_id rt1320_id = {
	.codec = {
		.version   = 0x30,
		.mfgid1    = 0x02,
		.mfgid2    = 0x5d,
		.partid1   = 0x13,
		.partid2   = 0x20,
		.sndwclass = 0x01,
	}
};

/*
 * SoundWire messages used to program the ALC1320 amplifier registers for initialization.
 */
static const sndw_cmd alc1320_beep_init_cmds[] = {
	{ .cmdtype = cmd_write, .regaddr = 0x0048, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0x0049, .regdata = 0x01, },
	{ .cmdtype = cmd_write, .regaddr = 0xc570, .regdata = 0x0a, },
	{ .cmdtype = cmd_write, .regaddr = 0x0048, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0x0049, .regdata = 0x01, },
	{ .cmdtype = cmd_read,  .regaddr = 0xc560, },
	{ .cmdtype = cmd_write, .regaddr = 0x0048, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0x0049, .regdata = 0x01, },
	{ .cmdtype = cmd_write, .regaddr = 0xc560, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0x0048, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0x0049, .regdata = 0x01, },
	{ .cmdtype = cmd_write, .regaddr = 0xc000, .regdata = 0x03, },
	{ .cmdtype = cmd_write, .regaddr = 0xc003, .regdata = 0xe0, },
	{ .cmdtype = cmd_write, .regaddr = 0xc01b, .regdata = 0xff, },
	{ .cmdtype = cmd_write, .regaddr = 0xc5c3, .regdata = 0xf3, },
	{ .cmdtype = cmd_write, .regaddr = 0xc057, .regdata = 0x51, },
	{ .cmdtype = cmd_write, .regaddr = 0xc054, .regdata = 0x35, },
	{ .cmdtype = cmd_write, .regaddr = 0xc604, .regdata = 0x40, },
	{ .cmdtype = cmd_write, .regaddr = 0xc609, .regdata = 0x40, },
	{ .cmdtype = cmd_write, .regaddr = 0xc600, .regdata = 0x05, },
	{ .cmdtype = cmd_write, .regaddr = 0xc601, .regdata = 0x80, },
	{ .cmdtype = cmd_write, .regaddr = 0xc500, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xc680, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xc681, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xc046, .regdata = 0xfc, },
	{ .cmdtype = cmd_write, .regaddr = 0xc045, .regdata = 0xff, },
	{ .cmdtype = cmd_write, .regaddr = 0xc044, .regdata = 0xff, },
	{ .cmdtype = cmd_write, .regaddr = 0xc043, .regdata = 0xff, },
	{ .cmdtype = cmd_write, .regaddr = 0xc042, .regdata = 0xff, },
	{ .cmdtype = cmd_write, .regaddr = 0xc041, .regdata = 0x7f, },
	{ .cmdtype = cmd_write, .regaddr = 0xc040, .regdata = 0xff, },
	{ .cmdtype = cmd_write, .regaddr = 0xc570, .regdata = 0x0b, },
	{ .cmdtype = cmd_write, .regaddr = 0xe802, .regdata = 0xf8, },
	{ .cmdtype = cmd_write, .regaddr = 0xe803, .regdata = 0xbe, },
	{ .cmdtype = cmd_write, .regaddr = 0xc003, .regdata = 0xc0, },
	{ .cmdtype = cmd_write, .regaddr = 0xd470, .regdata = 0xb1, },
	{ .cmdtype = cmd_write, .regaddr = 0xd471, .regdata = 0x1c, },
	{ .cmdtype = cmd_write, .regaddr = 0xc019, .regdata = 0x10, },
	{ .cmdtype = cmd_write, .regaddr = 0xd487, .regdata = 0x0b, },
	{ .cmdtype = cmd_write, .regaddr = 0xd487, .regdata = 0x3b, },
	{ .cmdtype = cmd_write, .regaddr = 0xd486, .regdata = 0xc3, },
	{ .cmdtype = cmd_write, .regaddr = 0x0048, .regdata = 0x20, },
	{ .cmdtype = cmd_write, .regaddr = 0x0049, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xf000, .regdata = 0x67, },
	{ .cmdtype = cmd_write, .regaddr = 0xf001, .regdata = 0x80, },
	{ .cmdtype = cmd_write, .regaddr = 0xf002, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xf003, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0x0048, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0x0049, .regdata = 0x01, },
	{ .cmdtype = cmd_write, .regaddr = 0xd486, .regdata = 0x43, },
	{ .cmdtype = cmd_write, .regaddr = 0x0048, .regdata = 0x20, },
	{ .cmdtype = cmd_write, .regaddr = 0x0049, .regdata = 0x01, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb00, .regdata = 0x04, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb01, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb02, .regdata = 0x11, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb03, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb04, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb05, .regdata = 0x82, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb06, .regdata = 0x04, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb07, .regdata = 0xf1, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb08, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb09, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb0a, .regdata = 0x40, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb0b, .regdata = 0x02, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb0c, .regdata = 0xf2, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb0d, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb0e, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb0f, .regdata = 0xe0, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb10, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb11, .regdata = 0x10, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb12, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb13, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xdb14, .regdata = 0x45, },
	{ .cmdtype = cmd_write, .regaddr = 0x0048, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0x0049, .regdata = 0x01, },
	{ .cmdtype = cmd_write, .regaddr = 0xd540, .regdata = 0x21, },
	{ .cmdtype = cmd_write, .regaddr = 0x0048, .regdata = 0x82, },
	{ .cmdtype = cmd_write, .regaddr = 0x0049, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0x9988, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0x8189, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0x818a, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0x0048, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0x0049, .regdata = 0x01, },
	{ .cmdtype = cmd_write, .regaddr = 0xc5fb, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xcf02, .regdata = 0x0f, },
	{ .cmdtype = cmd_write, .regaddr = 0xde02, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0xde03, .regdata = 0x0d, },
	{ .cmdtype = cmd_write, .regaddr = 0xc570, .regdata = 0x0a, },
	{ .cmdtype = cmd_write, .regaddr = 0xc01a, .regdata = 0x41, },
	{ .cmdtype = cmd_write, .regaddr = 0xc044, .regdata = 0x1f, },
	{ .cmdtype = cmd_write, .regaddr = 0xc041, .regdata = 0xff, },
	{ .cmdtype = cmd_write, .regaddr = 0xde00, .regdata = 0x1f, },
	{ .cmdtype = cmd_write, .regaddr = 0xde05, .regdata = 0x03, },
	{ .cmdtype = cmd_write, .regaddr = 0xde04, .regdata = 0xff, }
};

/*
 * SoundWire messages used to program the ALC1320 amplifier registers for exit.
 */
static const sndw_cmd alc1320_beep_exit_cmds[] = {
	{ .cmdtype = cmd_write, .regaddr = 0x0048, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0x0049, .regdata = 0x01, },
	{ .cmdtype = cmd_write, .regaddr = 0xc570, .regdata = 0x0a, },
	{ .cmdtype = cmd_read,  .regaddr = 0xc560, },
	{ .cmdtype = cmd_write, .regaddr = 0x0048, .regdata = 0x00, },
	{ .cmdtype = cmd_write, .regaddr = 0x0049, .regdata = 0x01, },
	{ .cmdtype = cmd_write, .regaddr = 0xc000, .regdata = 0x03, }
};

/*
 * SoundWire messages used to program the ALC1320 amplifier registers to
 * generate the boot beep via the tone generator.
 */
static const sndw_cmd alc1320_addr_page1_cmd = {
		.cmdtype = cmd_write,
		.regaddr = RT1320_ADDRPAGE1,
		.regdata = 0x00,
};

static const sndw_cmd alc1320_addr_page2_cmd = {
		.cmdtype = cmd_write,
		.regaddr = RT1320_ADDRPAGE2,
		.regdata = 0x01,
};

static const sndw_cmd alc1320_volume_cmd = {
		.cmdtype = cmd_write,
		.regaddr = RT1320_AMP_VOLUME,
		.regdata = RT1320_VOLUME_MINUS_15dB,
};

static const sndw_cmd alc1320_beep_on_cmd = {
		.cmdtype = cmd_write,
		.regaddr = RT1320_TONE_GENERATOR,
		.regdata = RT1320_TONE_GENERATOR_ENABLE,
};

static const sndw_cmd alc1320_beep_off_cmd = {
		.cmdtype = cmd_write,
		.regaddr = RT1320_TONE_GENERATOR,
		.regdata = RT1320_TONE_GENERATOR_DISABLE,
};

static const sndw_cmd alc1320_mute_off_cmd = {
		.cmdtype = cmd_write,
		.regaddr = RT1320_AMP_MUTE,
		.regdata = RT1320_MUTE_DISABLE,
};

static const sndw_cmd alc1320_mute_c570_off_cmd = {
		.cmdtype = cmd_write,
		.regaddr = RT1320_AMP_MUTE_C570,
		.regdata = RT1320_MUTE_C570_DISABLE,
};

static const sndw_cmd alc1320_mute_on_cmd = {
		.cmdtype = cmd_write,
		.regaddr = RT1320_AMP_MUTE,
		.regdata = RT1320_MUTE_ENABLE,
};

static const sndw_cmd alc1320_mute_c570_on_cmd = {
		.cmdtype = cmd_write,
		.regaddr = RT1320_AMP_MUTE_C570,
		.regdata = RT1320_MUTE_C570_ENABLE,
};

/*
 * rt1320_wait_ready_with_ack - Waits for the codec function to be ready.
 * sndw - Pointer to the Soundwire ops.
 * sndwlinkaddr - Soundwire controller link address.
 * txcmds - Pointer to send messages. (typically a read command).
 * sndwendpointdev - Endpoint device number for the amplifier.
 */
static int rt1320_wait_ready_with_ack(SndwOps *sndw, void *sndwlinkaddr, sndw_cmd txcmd,
					uint32_t sndwendpointdev)
{
	uint8_t val = 0;

	mdelay(1);
	for (int i = 0; i < 10; i++) {
		if (sndw->sndw_sendwack(sndwlinkaddr, txcmd, sndwendpointdev, &val) == 0) {
			if ((val & 0x07) == 0x00)
				return 0;
		}
		udelay(100);
	}

	printf("No ACK received after 10 retries.\n");
	return -1;
}

/*
 * rt1320_beep_init - Function to program the registers for beep generation.
 * sndw - Pointer to the Soundwire ops.
 * sndwlinkaddr - Soundwire controller link address.
 * sndwendpointdev - Endpoint device number for the amplifier.
 */
static int rt1320_beep_init(SndwOps *sndw, void *sndwlinkaddr, uint32_t sndwendpointdev)
{
	for (int i = 0; i < ARRAY_SIZE(alc1320_beep_init_cmds); i++) {
		const sndw_cmd *cmd = &alc1320_beep_init_cmds[i];
		if (cmd->cmdtype == cmd_read) {
			if (rt1320_wait_ready_with_ack(sndw, sndwlinkaddr,
							*cmd, sndwendpointdev)) {
				printf("Failed to read register.\n");
				return -1;
			}
		} else {
			if (sndw->sndw_sendwack(sndwlinkaddr, *cmd, sndwendpointdev, NULL)) {
				printf("Failed to write register.\n");
				return -1;
			}
		}
	}

	return 0;
}

/*
 * rt1320_beep_exit - Function to program the amplifier registers for exiting beep mode.
 * sndw - Pointer to the Soundwire ops.
 * sndwlinkaddr - Soundwire controller link address.
 * sndwendpointdev - Endpoint device number for the amplifier.
 */
static int rt1320_beep_exit(SndwOps *sndw, void *sndwlinkaddr, uint32_t sndwendpointdev)
{

	for (int i = 0; i < ARRAY_SIZE(alc1320_beep_exit_cmds); i++) {
		const sndw_cmd *cmd = &alc1320_beep_exit_cmds[i];
		if (cmd->cmdtype == cmd_read) {
			if (rt1320_wait_ready_with_ack(sndw, sndwlinkaddr,
							*cmd, sndwendpointdev)) {
				printf("Failed to read register.\n");
				return -1;
				}
		} else {
			if (sndw->sndw_sendwack(sndwlinkaddr, *cmd, sndwendpointdev, NULL)) {
				printf("Failed to write register.\n");
				return -1;
			}
		}
	}

	return 0;
}

/*
 * sndw_beep - Function program the registers to turn on the tone generator
 * in the amplifier.
 * sndw - Pointer to the Soundwire ops.
 * sndwlinkaddr - Soundwire controller link address.
 * sndwendpointdev - Endpoint device number for the amplifier.
 */
static int sndw_beep(SndwOps *sndw, void *sndwlinkaddr, uint32_t sndwendpointdev,
			uint32_t beep_duration)
{
	printf("%s start\n", __func__);

	/* Beep initialization */
	if (rt1320_beep_init(sndw, sndwlinkaddr, sndwendpointdev)) {
		printf("ALC1320 beep init failed. Aborting amplifier initialization.\n");
		return -1;
	}

	/* Send the cmd to write to address page 1 register */
	if (sndw->sndw_sendwack(sndwlinkaddr, alc1320_addr_page1_cmd,
				sndwendpointdev, NULL)) {
		printf("Failed to send the addr page1 command.\n");
		return -1;
	}
	/* Send the cmd to write to address page 2 register */
	if (sndw->sndw_sendwack(sndwlinkaddr, alc1320_addr_page2_cmd,
				sndwendpointdev, NULL)) {
		printf("Failed to send the addr page2 command.\n");
		return -1;
	}
	/* Send the cmd to write beep volume */
	if (sndw->sndw_sendwack(sndwlinkaddr, alc1320_volume_cmd,
				sndwendpointdev, NULL)) {
		printf("Failed to write beep volume.\n");
		return -1;
	}
	/* Send the cmd to disable the mute */
	if (sndw->sndw_sendwack(sndwlinkaddr, alc1320_mute_off_cmd,
				sndwendpointdev, NULL)) {
		printf("Failed to send the mute OFF command.\n");
		return -1;
	}
	/* Send the cmd to disable the mute */
	if (sndw->sndw_sendwack(sndwlinkaddr, alc1320_mute_c570_off_cmd,
				sndwendpointdev, NULL)) {
		printf("Failed to send the mute OFF command.\n");
		return -1;
	}
	/* Send the cmd to enable the tone generator */
	if (sndw->sndw_sendwack(sndwlinkaddr, alc1320_beep_on_cmd,
				sndwendpointdev, NULL)) {
		printf("Failed to send the beep ON command.\n");
		return -1;
	}
	/* Beep duration */
	mdelay(beep_duration);
	/* Send the cmd to disable the tone generator */
	if (sndw->sndw_sendwack(sndwlinkaddr, alc1320_beep_off_cmd,
				sndwendpointdev, NULL)) {
		printf("Failed to send the beep OFF command.\n");
		return -1;
	}
	/* Send the cmd to enable the mute */
	if (sndw->sndw_sendwack(sndwlinkaddr, alc1320_mute_on_cmd,
				sndwendpointdev, NULL)) {
		printf("Failed to send the mute ON command.\n");
		return -1;
	}
	/* Send the cmd to enable the mute */
	if (sndw->sndw_sendwack(sndwlinkaddr, alc1320_mute_c570_on_cmd,
				sndwendpointdev, NULL)) {
		printf("Failed to send the mute ON command.\n");
		return -1;
	}
	/*  Beep exit */
	if (rt1320_beep_exit(sndw, sndwlinkaddr, sndwendpointdev)) {
		printf("ALC1320 beep exit failed.\n");
		return -1;
	}

	printf("%s end\n", __func__);
	return 0;
}

/*
 * rt1320sndw_beep - Function involves enabling of the amplifier,
 * generate the beep via tone generator and then resets the DSP.
 */
static int rt1320sndw_beep(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	rt1320sndw *codec = container_of(me, rt1320sndw, ops);
	sndw_codec_info info;

	if (codec->sndw->sndw_enable(codec->sndw, &info)) {
		printf("Failed to enable codec.\n");
		return -1;
	}

	for (int i = 0; i < SNDW_DEV_ID_NUM; i++) {
		if (rt1320_id.id[i] != info.codecid.id[i]) {
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

/*
 * new_rt1320_sndw - new structure for Soundwire rt1320 amplifier.
 * sndw - Pointer to the Soundwire ops.
 */
rt1320sndw *new_rt1320_sndw(SndwOps *sndw, uint32_t beep_duration)
{
	rt1320sndw *codec = xzalloc(sizeof(*codec));
	codec->ops.play = &rt1320sndw_beep;
	codec->sndw = sndw;
	codec->beep_duration = beep_duration;

	return codec;
}

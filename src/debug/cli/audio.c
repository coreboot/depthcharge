/*
 * Copyright 2015 Chromium OS Authors
 * Command for testing audio driver.
 */

#include <drivers/sound/sound.h>
#include "common.h"

/* Do not play sounds for longer than this number of milliseconds. */
#define MAX_DURATION_MS (5 * 1000)

/* Developer mode beep details */
#define DEV_BEEP_FREQUENCY 400
#define DEV_BEEP_DURATION  120

static int beep(unsigned long freq, unsigned long duration)
{
	int res = sound_start(freq);
	if (!res) {
		/* Nonblocking API must be used. */
		u64 target_time = timer_us(0) + duration * 1000;

		/* Wait in case this was an non-blocking call. */
		while (timer_us(0) < target_time)
			;

		res = sound_stop();
		if (res) {
			printf("attempt to stop failed with %d\n", res);
			return CMD_RET_FAILURE;
		}
	} else {
		res = sound_play(duration, freq);
		if (res) {
			printf("error: attempt to play failed with %d\n", res);
			return CMD_RET_FAILURE;
		}
	}

	return CMD_RET_SUCCESS;
}

static int do_devbeep(cmd_tbl_t *cmbtp, int flag, int argc, char * const argv[])
{
	u64 target_time;

	/* First beep */
	beep(DEV_BEEP_FREQUENCY, DEV_BEEP_DURATION);

	/* Wait for duration */
	target_time = timer_us(0) + (DEV_BEEP_DURATION * 1000);
	while (timer_us(0) < target_time)
		;

	/* Second beep */
	beep(DEV_BEEP_FREQUENCY, DEV_BEEP_DURATION);

	return CMD_RET_SUCCESS;
}

static int do_audio(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned long freq;
	unsigned long duration;

	if (argc < 3) {
		printf("Required command line parameters missing\n");
		return CMD_RET_USAGE;
	}

	freq = strtoul(argv[1], 0, 10);
	duration = strtoul(argv[2], 0, 10);

	if ((freq < 20) || (freq > 20000)) {
		printf("Invalid frequency %ld Hertz\n", freq);
		return CMD_RET_FAILURE;
	}

	if (!duration)
		return CMD_RET_SUCCESS;

	if (duration > MAX_DURATION_MS) {
		printf("Capping duration from %ld to %d milliseconds\n",
		       duration, MAX_DURATION_MS);
		duration = MAX_DURATION_MS;
	}

	if (argc > 3) {
		uint32_t volume = (uint32_t) strtoul(argv[3], 0, 10);

		printf("Setting volume to %d\n", volume);
		sound_set_volume(volume);
	}

	return beep(freq, duration);
}

U_BOOT_CMD(
	   audio,	4,	1,
	   "rudimentary audio capabilities test",
	   "\n<freq> <duration> [<volume>]  - play sound of <freq> Hz for "
	   "<duration>\n"
	   "                                milliseconds at volume <volume>"
);

U_BOOT_CMD(
	   devbeep,	1,	1,
	   "developer mode timeout beep",
	   "\n"
);

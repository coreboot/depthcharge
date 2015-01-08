/*
 * Copyright 2015 Chromium OS Authors
 * Command for testing audio driver.
 */

#include <drivers/sound/sound.h>
#include "common.h"

/* Do not play sounds for longer than this number of milliseconds. */
#define MAX_DURATION_MS (5 * 1000)

static int do_audio(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned long freq;
	unsigned long duration;
	int res;
	int rv = 0;

	if (argc < 3) {
		printf("Requred command line parameters missing\n");
		return 1;
	}

	freq = strtoul(argv[1], 0, 10);
	duration = strtoul(argv[2], 0, 10);

	if ((freq < 20) || (freq > 20000)) {
		printf("will not try playing sound at %ld Hertz\n", freq);
		return 1;
	}

	if (!duration)
		return 0;

	if (duration > MAX_DURATION_MS) {
		printf("Capping duration from %ld to %d milliseconds\n",
		       duration, MAX_DURATION_MS);
		duration = MAX_DURATION_MS;
	}

	res = sound_start(freq);
	if (!res) {
		/* Nonblocking API must be used. */
		u64 target_time = timer_us(0) + duration * 1000;

		/* Wait in case this was an non-blocking call. */
		while (timer_us(0) < target_time)
			;

		res = sound_stop();
		if (res) {
			printf("attempt to stop failed with %d\n", res);
			rv = 1;
		}
	} else {
		res = sound_play(duration, freq);
		if (res) {
			printf("error: attempt to play failed with %d\n", res);
			rv = 1;
		}
	}


	return rv;
}

U_BOOT_CMD(
	   audio,	3,	1,
	   "rudimentary audio capabilities test",
	   "<freq> <duration>  - play sound of <freq> Hz "
	   "for <duration> milliseconds"
);

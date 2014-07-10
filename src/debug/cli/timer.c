/*
 * Command for testing system timer.
 *
 * Copyright (C) 2014 Chromium OS Authors
 */

#include "common.h"

static int do_time(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int counter = strtoul(argv[1], 0, 10);
	u64 now = timer_us(0);
	u64 next = 0;

	while (counter--) {
		next += 1000000; // That's one second

		while(timer_us(now) < next)
			;

		printf(" %d", counter);
		if (havekey()) {
			getchar();
			break;
		}
	}
	printf("\n");

	return 0;
}

U_BOOT_CMD(
	   time,	2,	1,
	   "rudimentary timer test",
	   "<count>  - print a line each second for <count> seconds"
);

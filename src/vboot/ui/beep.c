// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "drivers/sound/sound.h"
#include "vboot/ui.h"

void ui_beep(uint32_t msec, uint32_t frequency)
{
	uint64_t start;
	uint64_t elapsed;
	int ret;
	int need_stop = 1;

	/* Zero-length beep should return immediately. */
	if (msec == 0)
		return;

	start = timer_us(0);
	ret = sound_start(frequency);

	if (ret < 0) {
		/* Driver only supports blocking beep calls. */
		need_stop = 0;
		ret = sound_play(msec, frequency);
		if (ret)
			UI_WARN("WARNING: sound_play() returned %#x\n", ret);
	}

	/* Wait for requested time on a non-blocking call, and enforce minimum
	   delay in case of buggy sound drivers. */
	elapsed = timer_us(start);
	if (elapsed < msec * USECS_PER_MSEC)
		mdelay(msec - elapsed / USECS_PER_MSEC);

	if (need_stop)
		sound_stop();
}

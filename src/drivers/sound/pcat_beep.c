/*
 * Copyright (c) 2013 Google, Inc
 * Copyright (c) 2013 Sage Electronic Engineering, LLC.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <arch/io.h>
#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/sound/pcat_beep.h"

/* Timer 2 legacy PC beep functions */
static int pcat_timer_init(PcAtBeep *beep)
{
	if (beep->timer_init_done)
		return 0;
	/*
	 * Initialize PIT timer 2 to drive the speaker
	 * (to start a beep: write 3 to port 0x61,
	 * to stop it again: write 0)
	 */
	outb(PitCmdCtr2 | PitCmdBoth | PitCmdMode3, PitBaseAddr + PitCommand);
	outb(PitTimer2Value, PitBaseAddr + PitT2);
	outb(PitTimer2Value >> 8, PitBaseAddr + PitT2);
	beep->timer_init_done = 1;
	return 0;
}

static int pcat_beep_start(SoundOps *me, uint32_t frequency)
{
	PcAtBeep *beep = container_of(me, PcAtBeep, ops);
	uint32_t countdown;

	if (!frequency)
		return -1;
	countdown = PitHz / frequency;

	pcat_timer_init(beep);
	outb(PitCmdCtr2 | PitCmdBoth | PitCmdMode3, PitBaseAddr + PitCommand);
	outb(countdown & 0xff, PitBaseAddr + PitT2);
	outb((countdown >> 8) & 0xff , PitBaseAddr + PitT2);
	outb(inb(PitPortBAddr) | PitPortBBeepEnable, PitPortBAddr);
	return 0;
}

static int pcat_beep_stop(SoundOps *me)
{
	PcAtBeep *beep = container_of(me, PcAtBeep, ops);
	pcat_timer_init(beep);
	outb(inb(PitPortBAddr) & ~PitPortBBeepEnable, PitPortBAddr);
	return 0;
}

static int pcat_beep_play(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	int res = sound_start(frequency);
	mdelay(msec);
	res |= sound_stop();
	return res;
}

PcAtBeep *new_pcat_beep(void)
{
	PcAtBeep *beep = xzalloc(sizeof(*beep));
	beep->ops.start = &pcat_beep_start;
	beep->ops.stop = &pcat_beep_stop;
	beep->ops.play = &pcat_beep_play;
	beep->timer_init_done = 0;
	return beep;
}

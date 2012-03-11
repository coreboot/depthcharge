/*
 * Copyright (c) 2012 The Chromium OS Authors.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload-config.h>
#include <libpayload.h>

#include <arch/rdtsc.h>
#include <hda_codec.h>
#include <vboot_api.h>

uint64_t VbExGetTimer(void)
{
	return rdtsc();
}

void VbExSleepMs(uint32_t msec)
{
	mdelay(msec);
}

VbError_t VbExBeep(uint32_t msec, uint32_t frequency)
{
        if (frequency)
                enable_beep(frequency);
        else
                disable_beep();

        if (msec > 0) {
                mdelay(msec);
                disable_beep();
        }

        return VBERROR_SUCCESS;
}

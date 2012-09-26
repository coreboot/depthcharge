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

#include <vboot_api.h>

static const uint32_t CSI_0 = 0x1B;
static const uint32_t CSI_1 = 0x5B;

uint32_t VbExKeyboardRead(void)
{
	uint32_t c;

	// No input, just give up.
	if (!havechar())
		return 0;

	c = getchar();
	// Handle a non-escape character or a standalone escape character.
	if (c != CSI_0 || !havechar()) {
		// Translate enter.
		if (c == '\n')
			return VB_KEY_CTRL_ENTER;
		return c;
	}

	// Ignore non escape [ sequences.
	if (getchar() != CSI_1)
		return 0;

	// Translate the arrow keys, and ignore everything else.
	switch (getchar()) {
	case 'A': return VB_KEY_UP;
	case 'B': return VB_KEY_DOWN;
	case 'C': return VB_KEY_RIGHT;
	case 'D': return VB_KEY_LEFT;
	default: return 0;
	}
}

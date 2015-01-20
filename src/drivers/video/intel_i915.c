/*
 * Copyright 2015 Google Inc.
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

#include <libpayload.h>
#include <stdint.h>

#include "drivers/video/display.h"
#include "drivers/video/intel_i915.h"

enum {
	/* Backlight control */
	BLC_PCH_PWM_CTL1   = 0xc8250,
	BLC_PCH_PWM_ENABLE = (1 << 31),
};

static uintptr_t reg_base;

int intel_i915_display_stop(void)
{
	uint32_t blc = readl(reg_base + BLC_PCH_PWM_CTL1);

	/* Turn off backlight if enabled */
	if (blc & BLC_PCH_PWM_ENABLE) {
		blc &= ~BLC_PCH_PWM_ENABLE;
		writel(blc, reg_base + BLC_PCH_PWM_CTL1);
	}

	return 0;
}

static DisplayOps intel_i915_display_ops= {
	.stop = intel_i915_display_stop,
};

DisplayOps *new_intel_i915_display(uintptr_t reg_addr)
{
	reg_base = reg_addr;
	return &intel_i915_display_ops;
}

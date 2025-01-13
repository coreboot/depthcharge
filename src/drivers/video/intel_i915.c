/*
 * Copyright 2015 Google LLC
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

static void *reg_base;

static int intel_i915_display_stop(DisplayOps *me)
{
	uint32_t blc = read32(reg_base + BLC_PCH_PWM_CTL1);

	/* Turn off backlight if enabled */
	if (blc & BLC_PCH_PWM_ENABLE) {
		blc &= ~BLC_PCH_PWM_ENABLE;
		write32(reg_base + BLC_PCH_PWM_CTL1, blc);
	}

	return 0;
}

static DisplayOps intel_i915_display_ops= {
	.stop = intel_i915_display_stop,
};

DisplayOps *new_intel_i915_display(uintptr_t reg_addr)
{
	reg_base = (void *)reg_addr;
	return &intel_i915_display_ops;
}

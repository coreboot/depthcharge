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
	/* Panel Power Control */
	PP_CONTROL_REG               = 0xc7204,
	PP_CONTROL_PANEL_UNLOCK_MASK = 0xffff0000,
	PP_CONTROL_PANEL_UNLOCK_REGS = 0xabcd0000,
	PP_CONTROL_POWER_TARGET_ON   = (1 << 0),
	PP_CONTROL_PANEL_POWER_RESET = (1 << 1),
	PP_CONTROL_EDP_BLC_ENABLE    = (1 << 2),
	PP_CONTROL_EDP_FORCE_VDD     = (1 << 3),

	/* Panel Power Off Delays */
	PP_OFF_DELAYS_REG            = 0xc720c,
	PP_OFF_DELAYS_BL_OFF_MASK    = 0x1fff,
};

static uintptr_t reg_base;

int intel_i915_display_stop(void)
{
	uint32_t pp_ctrl, bl_off_delay;

	pp_ctrl = readl(reg_base + PP_CONTROL_REG);

	/* Check if backlight is enabled */
	if (!(pp_ctrl & PP_CONTROL_EDP_BLC_ENABLE))
		return 0;

	/* Enable writes to this register */
	pp_ctrl &= ~PP_CONTROL_PANEL_UNLOCK_MASK;
	pp_ctrl |= PP_CONTROL_PANEL_UNLOCK_REGS;

	/* Switch off panel power and force VDD */
	pp_ctrl &= ~PP_CONTROL_EDP_BLC_ENABLE;

	writel(pp_ctrl, reg_base + PP_CONTROL_REG);
	readl(reg_base + PP_CONTROL_REG);

	/* Read backlight off delay in 100us units */
	bl_off_delay = readl(reg_base + PP_OFF_DELAYS_REG);
	bl_off_delay &= PP_OFF_DELAYS_BL_OFF_MASK;
	bl_off_delay *= 100;

	/* Wait for backlight to turn off */
	udelay(bl_off_delay);

	/* Switch off panel power and force VDD */
	pp_ctrl &= ~PP_CONTROL_POWER_TARGET_ON;
	pp_ctrl &= ~PP_CONTROL_PANEL_POWER_RESET;
	pp_ctrl &= ~PP_CONTROL_EDP_FORCE_VDD;

	writel(pp_ctrl, reg_base + PP_CONTROL_REG);
	readl(reg_base + PP_CONTROL_REG);

	printf("Panel turned off\n");

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

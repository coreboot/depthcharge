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
#include "drivers/power/armada38x.h"

#define MV_MISC_REGS_BASE			(0xF1018200)
#define CPU_RSTOUTN_MASK_REG			(MV_MISC_REGS_BASE + 0x60)
#define CPU_SYS_SOFT_RST_REG			(MV_MISC_REGS_BASE + 0x64)

static int armada38x_reboot(struct PowerOps *me)
{
	//software reset
	setbits32p(CPU_RSTOUTN_MASK_REG, 1 << 0);
	setbits32p(CPU_SYS_SOFT_RST_REG, 1 << 0);
	for (;;)
		;
	return 0;
}

static int armada38x_power_off(struct PowerOps *me)
{
	die("Power off not implemented\n");
}

PowerOps *new_armada38x_power_ops(void)
{
	PowerOps *pops = xzalloc(sizeof(*pops));

	pops->reboot = armada38x_reboot;
	pops->power_off = armada38x_power_off;

	return pops;
}

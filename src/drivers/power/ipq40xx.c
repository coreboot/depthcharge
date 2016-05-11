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
#include "base/container_of.h"
#include "drivers/power/power.h"
#include "board/gale/board.h"

#define GCNT_PSHOLD		((void *)0x004AB000u)


static int ipq40xx_cold_reboot(struct PowerOps *me)
{
	writel(0, TCSR_BOOT_MISC_DETECT);
	writel(0, TCSR_RESET_DEBUG_SW_ENTRY);
	writel(0, GCNT_PSHOLD);

	for (;;)
		;
	return 0;
}

static int ipq40xx_power_off(struct PowerOps *me)
{
	printf("Power off not yet implemented, invoking cold_reboot().\n");
	return ipq40xx_cold_reboot(me);
}

PowerOps *new_ipq40xx_power_ops(void)
{
	PowerOps *pops = xzalloc(sizeof(*pops));

	pops->cold_reboot = ipq40xx_cold_reboot;
	pops->power_off = ipq40xx_power_off;

	return pops;
}

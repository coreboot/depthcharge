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
 */

#include <libpayload.h>
#include "base/container_of.h"
#include "drivers/power/ipq806x.h"

/*
 * Watchdog bark time value is kept five times larger than the default
 * watchdog timeout of 0x31F3, effectively disabling the watchdog bark
 * interrupt.
 */
#define RESET_WDT_BITE_TIME 0x31F3
#define RESET_WDT_BARK_TIME (5 * RESET_WDT_BITE_TIME)

static int ipq8086_cold_reboot(struct PowerOps *me)
{
	write32(APCS_WDT0_EN, 0);
	write32(APCS_WDT0_RST, 1);
	write32(APCS_WDT0_BARK_TIME, RESET_WDT_BARK_TIME);
	write32(APCS_WDT0_BITE_TIME, RESET_WDT_BITE_TIME);
	write32(APCS_WDT0_EN, 1);
	write32(APCS_WDT0_CPU0_WDOG_EXPIRED_ENABLE, 1);

	for (;;)
		;
	return 0;
}

static int ipq8086_power_off(struct PowerOps *me)
{
	printf("Power off not yet implemented, invoking cold_reboot().\n");
	return ipq8086_cold_reboot(me);
}

PowerOps *new_ipq806x_power_ops(void)
{
	PowerOps *pops = xzalloc(sizeof(*pops));

	pops->cold_reboot = ipq8086_cold_reboot;
	pops->power_off = ipq8086_power_off;

	return pops;
}

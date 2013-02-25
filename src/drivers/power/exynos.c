/*
 * Copyright 2013 Google Inc.
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

#include "drivers/power/power.h"

void cold_reboot(void)
{
	uint32_t *inform1 = (uint32_t *)(uintptr_t)0x10040804;
	uint32_t *swreset = (uint32_t *)(uintptr_t)0x10040400;

	printf("Rebooting...\n");

	writel(0, inform1);
	writel(readl(swreset) | 1, swreset);

	halt();
}

void power_off(void)
{
	uint32_t *pshold = (uint32_t *)(uintptr_t)0x1004330c;
	writel(readl(pshold) & ~(1 << 8), pshold);
	halt();
}

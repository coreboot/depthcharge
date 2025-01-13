/*
 * Copyright 2013 Google LLC
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

#include "drivers/power/exynos.h"
#include "drivers/power/power.h"

static int exynos_reboot(PowerOps *me)
{
	uint32_t *inform1 = (uint32_t *)(uintptr_t)0x10040804;
	uint32_t *swreset = (uint32_t *)(uintptr_t)0x10040400;

	write32(inform1, 0);
	write32(swreset, read32(swreset) | 1);

	halt();
}

static int exynos_power_off(PowerOps *me)
{
	uint32_t *pshold = (uint32_t *)(uintptr_t)0x1004330c;
	write32(pshold, read32(pshold) & ~(1 << 8));
	halt();
}

PowerOps exynos_power_ops = {
	.reboot = &exynos_reboot,
	.power_off = &exynos_power_off
};

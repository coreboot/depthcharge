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

#ifndef __DRIVERS_POWER_IPQ806X_H__
#define __DRIVERS_POWER_IPQ806X_H__

#include "drivers/power/power.h"

#define MSM_CLK_CTL_BASE    ((void *)0x00900000)
#define MSM_TMR_BASE        ((void *)0x0200A000)

#define APCS_WDT0_EN        (MSM_TMR_BASE + 0x0040)
#define APCS_WDT0_RST       (MSM_TMR_BASE + 0x0038)
#define APCS_WDT0_BARK_TIME (MSM_TMR_BASE + 0x004C)
#define APCS_WDT0_BITE_TIME (MSM_TMR_BASE + 0x005C)
#define APCS_WDT0_CPU0_WDOG_EXPIRED_ENABLE (MSM_CLK_CTL_BASE + 0x3820)

PowerOps *new_ipq806x_power_ops(void);

#endif /* __DRIVERS_POWER_IPQ806X_H__ */

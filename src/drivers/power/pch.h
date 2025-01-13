/*
 * Copyright 2012 Google LLC
 * Copyright (C) 2018 Intel Corporation.
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

#ifndef __DRIVERS_POWER_PCH_H__
#define __DRIVERS_POWER_PCH_H__

#include "drivers/power/power.h"

extern PowerOps pch_power_ops;
extern PowerOps alderlake_power_ops;
extern PowerOps baytrail_power_ops;
extern PowerOps braswell_power_ops;
extern PowerOps skylake_power_ops;
extern PowerOps cannonlake_power_ops;
extern PowerOps tigerlake_power_ops;
extern PowerOps icelake_power_ops;
extern PowerOps apollolake_power_ops;
extern PowerOps jasperlake_power_ops;

#endif /* __DRIVERS_POWER_PCH_H__ */

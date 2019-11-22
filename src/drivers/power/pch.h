/*
 * Copyright 2012 Google Inc.
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

PowerOps pch_power_ops;
PowerOps baytrail_power_ops;
PowerOps braswell_power_ops;
PowerOps skylake_power_ops;
PowerOps cannonlake_power_ops;
PowerOps icelake_power_ops;
PowerOps apollolake_power_ops;
PowerOps jasperlake_power_ops;

#endif /* __DRIVERS_POWER_PCH_H__ */

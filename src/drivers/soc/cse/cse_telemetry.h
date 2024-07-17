/*
 * Copyright 2024 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_SOC_CSE_CSE_TELEMETRY_H__
#define __DRIVERS_SOC_CSE_CSE_TELEMETRY_H__

#if CONFIG(SOC_INTEL_CSE_PRE_CPU_RESET_TELEMETRY_V1)
#include "cse_telemetry_v1.h"
#elif CONFIG(SOC_INTEL_CSE_PRE_CPU_RESET_TELEMETRY_V2)
#include "cse_telemetry_v2.h"
#endif

#endif // __DRIVERS_SOC_CSE_CSE_TELEMETRY_V2_H__
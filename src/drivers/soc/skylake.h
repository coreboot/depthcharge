/*
 * Copyright 2016 Google LLC
 * Copyright (C) 2015 Intel Corporation.
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

#ifndef __DRIVERS_SOC_SKYLAKE_H__
#define __DRIVERS_SOC_SKYLAKE_H__

#include <stdint.h>
#include "drivers/soc/common/intel_gpe.h"

/* Skylake PCR access to GPIO registers. */
#define PCH_PCR_BASE_ADDRESS	0xfd000000

/* PCR PIDs. */
#define PCH_PCR_PID_GPIOCOM3	0xAC
#define PCH_PCR_PID_GPIOCOM2	0xAD
#define PCH_PCR_PID_GPIOCOM1	0xAE
#define PCH_PCR_PID_GPIOCOM0	0xAF
#define PCH_PCR_PID_ITSS	0xC4

/* I2C Designware Controller runs at 120MHz */
#define SKYLAKE_DW_I2C_MHZ	120

/* GPE definitions */

int skylake_get_gpe(int gpe);

#define GPE0_STS_OFF		0x80

#endif /* __DRIVERS_SOC_SKYLAKE_H__ */

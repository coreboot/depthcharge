/*
 * Copyright 2020 Google LLC
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

#ifndef __DRIVERS_SOC_TIGERLAKE_H__
#define __DRIVERS_SOC_TIGERLAKE_H__

#include "drivers/soc/common/intel_gpe.h"

/* PCR Interface */
#define PCH_PCR_BASE_ADDRESS	0xfd000000
#define PCH_PCR_PID_GPIOCOM0	0x6e
#define PCH_PCR_PID_GPIOCOM1	0x6d
#define PCH_PCR_PID_GPIOCOM2	0x6c
#define PCH_PCR_PID_GPIOCOM3	0x6b
#define PCH_PCR_PID_GPIOCOM4	0x6a
#define PCH_PCR_PID_GPIOCOM5	0x69

/* I2C Designware Controller runs at 133MHz */
#define TIGERLAKE_DW_I2C_MHZ	133

/* GPE definitions */

int tigerlake_get_gpe(int gpe);

#define GPE0_STS_OFF		0x60

#endif /* __DRIVERS_SOC_TIGERLAKE_H__ */

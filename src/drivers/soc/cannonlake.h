/*
 * Copyright 2017 Google Inc.
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

#ifndef __DRIVERS_SOC_CANNONLAKE_H__
#define __DRIVERS_SOC_CANNONLAKE_H__

#include "drivers/soc/common/intel_gpe.h"

/* PCR Interface */
#define PCH_PCR_BASE_ADDRESS	0xfd000000
#define PCH_PCR_PID_GPIOCOM0	0x6e
#define PCH_PCR_PID_GPIOCOM1	0x6d
#define PCH_PCR_PID_GPIOCOM2	0x6c
#define PCH_PCR_PID_GPIOCOM3	0x6b
#define PCH_PCR_PID_GPIOCOM4	0x6a

/* I2C Designware Controller runs at 216MHz */
#define CANNONLAKE_DW_I2C_MHZ	216

/* GPE definitions */

int cannonlake_get_gpe(int gpe);

#define GPE0_STS_OFF             0x60

#endif /* __DRIVERS_SOC_CANNONLAKE_H__ */

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2020 Google Inc.
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

#ifndef __DRIVERS_STORAGE_HEALTH_H__
#define __DRIVERS_STORAGE_HEALTH_H__

#include "drivers/storage/mmc.h"
#include "drivers/storage/nvme.h"

typedef enum {
	HEALTH_UNKNOWN = 0,
	HEALTH_NVME,
	HEALTH_MMC,
} HealthDataFormat;

typedef struct HealthInfo {
	HealthDataFormat type;

	union {
#ifdef CONFIG_DRIVER_STORAGE_NVME
		NvmeSmartLogData nvme_data;
#endif
#ifdef CONFIG_DRIVER_STORAGE_MMC
		MmcHealthData mmc_data;
#endif
	} data;
} HealthInfo;
#endif

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2020 Google LLC
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

#ifndef __DRIVERS_STORAGE_INFO_H__
#define __DRIVERS_STORAGE_INFO_H__

#include "drivers/storage/mmc.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/ufs.h"

typedef enum {
	STORAGE_INFO_TYPE_UNKNOWN = 0,
	STORAGE_INFO_TYPE_NVME,
	STORAGE_INFO_TYPE_MMC,
	STORAGE_INFO_TYPE_UFS,
} StorageInfoType;

typedef struct HealthInfo {
	StorageInfoType type;

	union {
		NvmeSmartLogData nvme_data;
		MmcHealthData mmc_data;
		UfsDescHealth ufs_data;
	} data;
} HealthInfo;

typedef struct StorageTestLog {
	StorageInfoType type;

	union {
		NvmeTestLogData nvme_data;
		UfsTestLogData ufs_data;
	} data;
} StorageTestLog;

#endif

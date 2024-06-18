/*
 * Copyright 2012 Google LLC
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

#ifndef __BASE_TIMESTAMP_H__
#define __BASE_TIMESTAMP_H__

#include <stddef.h>
#include <stdint.h>

enum timestamp_id {
	/* 940-950 reserved for vendorcode extensions (940-950: Intel ME) */
	TS_ME_INFORM_DRAM_START = 940,
	TS_ME_INFORM_DRAM_END = 941,
	TS_ME_END_OF_POST_START = 942,
	TS_ME_END_OF_POST_END = 943,
	TS_ME_BOOT_STALL_END = 944,
	TS_ME_ICC_CONFIG_START = 945,
	TS_ME_HOST_BOOT_PREP_END = 946,
	TS_ME_RECEIVED_CRDA_FROM_PMC = 947,
	TS_CSE_FW_SYNC_START = 948,
	TS_CSE_FW_SYNC_END = 949,

	/* 990+ reserved for vendorcode extensions (990-999: Intel ME continued) */
	TS_ME_ROM_START = 990,
	TS_ISSE_DMU_LOAD_END = 991,

	// Depthcharge entry IDs start at 1000.
	TS_START = 1000,

	TS_RO_PARAMS_INIT = 1001,
	TS_RO_VB_INIT = 1002,
	TS_RO_VB_SELECT_FIRMWARE = 1003,
	TS_RO_VB_SELECT_AND_LOAD_KERNEL = 1004,

	TS_RW_VB_SELECT_AND_LOAD_KERNEL = 1010,

	TS_VB_SELECT_AND_LOAD_KERNEL = 1020,
	TS_VB_EC_VBOOT_DONE = 1030,
	TS_VB_STORAGE_INIT_DONE = 1040,
	TS_VB_READ_KERNEL_DONE = 1050,
	TS_VB_AUXFW_SYNC_DONE = 1060,
	TS_VB_VBOOT_DONE = 1100,

	TS_START_KERNEL = 1101,
	TS_KERNEL_DECOMPRESSION = 1102,
};

void timestamp_init(void);
void timestamp_add(enum timestamp_id id, uint64_t ts_time);
void timestamp_add_now(enum timestamp_id id);
void timestamp_mix_in_randomness(u8 *buffer, size_t size);
uint64_t get_us_since_boot(void);
/* Returns timestamp tick frequency in MHz. */
int timestamp_tick_freq_mhz(void);

#endif /* __BASE_TIMESTAMP_H__ */

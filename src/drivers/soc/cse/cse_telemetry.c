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

#include "base/init_funcs.h"
#include "cse_internal.h"
#include "cse.h"
#include "cse_telemetry.h"

#define MSEC_TO_USEC(x) ((s64)x * 1000)

#if CONFIG(SOC_INTEL_CSE_PRE_CPU_RESET_TELEMETRY_V1)
static int soc_cbmem_inject_telemetry_data(s64 *time_stamp, s64 current_time)
{
	s64 start_stamp;

	if (!time_stamp) {
		printk(BIOS_ERR, "%s: Failed to insert CSME timestamps\n", __func__);
		return -1;
	}

	start_stamp = current_time - time_stamp[PERF_DATA_CSME_GET_PERF_RESPONSE];

	timestamp_add(TS_ME_ROM_START, start_stamp);
	timestamp_add(TS_ME_BOOT_STALL_END,
		start_stamp + time_stamp[PERF_DATA_CSME_RBE_BOOT_STALL_DONE_TO_PMC]);
	timestamp_add(TS_ME_ICC_CONFIG_START,
		start_stamp + time_stamp[PERF_DATA_CSME_POLL_FOR_PMC_PPS_START]);
	timestamp_add(TS_ME_HOST_BOOT_PREP_END,
		start_stamp + time_stamp[PERF_DATA_CSME_HOST_BOOT_PREP_DONE]);
	timestamp_add(TS_ME_RECEIVED_CRDA_FROM_PMC,
		start_stamp + time_stamp[PERF_DATA_PMC_SENT_CRDA]);

	return 0;
}
#elif CONFIG(SOC_INTEL_CSE_PRE_CPU_RESET_TELEMETRY_V2)
static int soc_cbmem_inject_telemetry_data(s64 *time_stamp, s64 current_time)
{
	s64 start_stamp;

	if (!time_stamp) {
		printk(BIOS_ERR, "%s: Failed to insert CSME timestamps\n", __func__);
		return -1;
	}

	start_stamp = current_time - time_stamp[PERF_DATA_CSME_GET_PERF_RESPONSE];

	timestamp_add(TS_ME_ROM_START, start_stamp);
	timestamp_add(TS_ME_BOOT_STALL_END,
		start_stamp + time_stamp[PERF_DATA_CSME_RBE_BOOT_STALL_DONE_TO_PMC]);
	timestamp_add(TS_ME_ICC_CONFIG_START,
		start_stamp + time_stamp[PERF_DATA_CSME_GOT_ICC_CFG_START_MSG_FROM_PMC]);
	timestamp_add(TS_ME_HOST_BOOT_PREP_END,
		start_stamp + time_stamp[PERF_DATA_CSME_HOST_BOOT_PREP_DONE]);
	timestamp_add(TS_ME_RECEIVED_CRDA_FROM_PMC,
		start_stamp + time_stamp[PERF_DATA_PMC_SENT_CRDA]);
	timestamp_add(TS_ISSE_DMU_LOAD_END,
		start_stamp + time_stamp[PERF_DATA_ISSE_DMU_LOAD_COMPLETED]);

	return 0;

}
#elif CONFIG(SOC_INTEL_CSE_PRE_CPU_RESET_TELEMETRY_V3)
static int soc_cbmem_inject_telemetry_data(s64 *time_stamp, s64 current_time)
{
	s64 start_stamp;

	if (!time_stamp) {
		printk(BIOS_ERR, "%s: Failed to insert CSME timestamps\n", __func__);
		return -1;
	}

	start_stamp = current_time - time_stamp[PERF_DATA_CSME_GET_PERF_RESPONSE];

	timestamp_add(TS_ME_ROM_START, start_stamp);
	timestamp_add(TS_ME_BOOT_STALL_END,
		start_stamp + time_stamp[PERF_DATA_CSME_RBE_BOOT_STALL_DONE_TO_PMC]);
	timestamp_add(TS_ME_ICC_CONFIG_START,
		start_stamp + time_stamp[PERF_DATA_CSME_GOT_ICC_CFG_START_MSG_FROM_PMC]);
	timestamp_add(TS_ME_HOST_BOOT_PREP_END,
		start_stamp + time_stamp[PERF_DATA_CSME_HOST_BOOT_PREP_DONE]);
	timestamp_add(TS_ME_RECEIVED_CRDA_FROM_PMC,
		start_stamp + time_stamp[PERF_DATA_PMC_SENT_CRDA]);
	timestamp_add(TS_ESE_LOAD_AUNIT_END,
		start_stamp + time_stamp[PERF_DATA_ESE_LOAD_AUNIT_COMPLETED]);

	return 0;

}
#else
#error "Select a valid pre CPU reset telemetry config"
#endif

static int process_cse_telemetry_data(void)
{
	struct cse_boot_perf_rsp cse_perf_data;
	s64 time_stamp[NUM_CSE_BOOT_PERF_DATA] = {0};
	s64 current_time;
	int zero_point_idx = 0;

	/*
	 * 1. Each TS holds the time elapsed between the "Zero-Point" till the TS itself
	 *    happened.
	 * 2. In case CSME did not hit some of the TS in the latest boot flow that value of
	 *    these TS will be 0x00000000.
	 * 3. In case of error, TS value will be set to 0xFFFFFFFF.
	 * 4. All other TS values will be relative to the zero point. The API caller should
	 *    normalize the TS values to the zero-point value.
	 */
	if (cse_get_boot_performance_data(&cse_perf_data) != CB_SUCCESS)
		return -1;

	current_time = CONFIG(TIMESTAMP_RAW) ? timer_raw_value() : timer_us(0);

	for (unsigned int i = 0; i < NUM_CSE_BOOT_PERF_DATA; i++) {
		if (cse_perf_data.timestamp[i] == 0xffffffff) {
			printk(BIOS_ERR, "%s: CSME timestamps invalid\n", __func__);
			return -1;
		}

		time_stamp[i] = (s64)MSEC_TO_USEC(cse_perf_data.timestamp[i]) *
			timestamp_tick_freq_mhz();
	}

	/* Find zero-point */
	for (unsigned int i = 0; i < NUM_CSE_BOOT_PERF_DATA; i++) {
		if (cse_perf_data.timestamp[i] != 0) {
			zero_point_idx = i;
			break;
		}
	}

	/* Normalize TS values to zero-point */
	for (unsigned int i = zero_point_idx + 1; i < NUM_CSE_BOOT_PERF_DATA; i++) {
		if (time_stamp[i] && time_stamp[i] < time_stamp[zero_point_idx]) {
			printk(BIOS_ERR, "%s: CSME timestamps invalid,"
					" wraparound detected\n", __func__);
			return -1;
		}

		if (time_stamp[i])
			time_stamp[i] -= time_stamp[zero_point_idx];
	}

	/* Inject CSME timestamps into the coreboot timestamp table */
	return soc_cbmem_inject_telemetry_data(time_stamp, current_time);
}

static int do_get_cse_telemetry_data(void)
{
	/* If CSE is already hidden then accessing CSE registers should be avoided */
	if (!is_cse_enabled()) {
		printk(BIOS_DEBUG, "CSE is disabled, not sending `Get Boot Perf` message\n");
		return -1;
	}

	/* Update coreboot timestamp table with CSE timestamps */
	return process_cse_telemetry_data();
}

INIT_FUNC(do_get_cse_telemetry_data);

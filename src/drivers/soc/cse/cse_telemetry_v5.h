/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __DRIVERS_SOC_CSE_CSE_TELEMETRY_V5_H__
#define __DRIVERS_SOC_CSE_CSE_TELEMETRY_V5_H__

/* Number of CSE boot performance data entries */
#define NUM_CSE_BOOT_PERF_DATA	96

/*
 * NVL pre-cpu reset CSME boot performance data layout (version 5).
 *
 * The response carries up to 96 timestamps. Most slots are reserved; only
 * the entries that are consumed by the coreboot/depthcharge timestamp
 * injection are named here.
 */
enum cse_boot_perf_data_v5 {
	/* CSME.ROM started execution (N/A for NVL, always 0) */
	PERF_DATA_CSME_ROM_START = 0,

	/* 1 - 5 Reserved */

	/* CSME.RBE set "Boot Stall Done" indication to PMC */
	PERF_DATA_CSME_RBE_BOOT_STALL_DONE_TO_PMC = 6,

	/* 7 - 15 Reserved */

	/* CSME got ICC_CONFIG_START message from PMC */
	PERF_DATA_CSME_GOT_ICC_CFG_START_MSG_FROM_PMC = 16,

	/* CSME set "Host Boot Prep Done" indication to PMC */
	PERF_DATA_CSME_HOST_BOOT_PREP_DONE = 17,

	/* 18 - 30 Reserved */

	/* ESE completed DMU binaries loading */
	PERF_DATA_ESE_LOAD_DMU_COMPLETED = 31,

	/* 32 - 34 Reserved */

	/* PMC sent "Core Reset Done Ack - Sent" message to CSME */
	PERF_DATA_PMC_SENT_CRDA = 35,

	/* 36 Reserved (ESE started loading AUnit) */

	/* ESE completed AUnit binaries loading */
	PERF_DATA_ESE_LOAD_AUNIT_COMPLETED = 37,

	/* 38 - 94 Reserved */

	/* Timestamp when CSME responded to BupGetEarlyBootData message itself */
	PERF_DATA_CSME_GET_PERF_RESPONSE = 95,
};

#endif /* __DRIVERS_SOC_CSE_CSE_TELEMETRY_V5_H__ */

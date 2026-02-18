// SPDX-License-Identifier: GPL-2.0

#ifndef __QCOM_TSENS_H__
#define __QCOM_TSENS_H__

#include <libpayload.h>

/* --- Register Offsets --- */
#define TM_SN_STATUS_OFF                0x00a0
#define SN_STATUS_VALID                 (1 << 21)
#define SN_STATUS_TEMP_MASK             0xFFF

/* --- Configuration Structs --- */

struct tsens_controller {
	const char *name;
	uintptr_t tm_base;
	uintptr_t srot_base;
	uint32_t sensor_count;
};

enum sensor_type {
	TYPE_TSENS
};

struct thermal_zone_map {
	const char *label;
	enum sensor_type type;
	const void *ctrl;
	int hw_id;
	int threshold;
};

/* --- Prototypes --- */

/*
 * Iterates through all defined zones and checks thresholds.
 */
void qcom_tsens_monitor_all(bool *has_crossed_threshold);

/* --- External Data (Defined in SoC file) --- */
extern const struct thermal_zone_map qcom_thermal_zones[];

#endif /* __QCOM_TSENS_H__ */

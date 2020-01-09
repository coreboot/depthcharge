/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2019 Google LLC
 *
 * Common SoC driver for Intel SOC.
 */

#ifndef __DRIVERS_SOC_INTEL_COMMON_H__
#define __DRIVERS_SOC_INTEL_COMMON_H__

#include <stdint.h>

/* Platform specific GPE configuration */
typedef struct SocGpeConfig {
	int gpe_max;
	uint16_t acpi_base;
	uint16_t gpe0_sts_off;
} SocGpeConfig;

int soc_get_gpe(int gpe, const SocGpeConfig *cfg);

#endif /* __DRIVERS_SOC_INTEL_COMMON_H__ */

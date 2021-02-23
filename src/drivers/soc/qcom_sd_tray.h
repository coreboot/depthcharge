/* SPDX-License-Identifier: GPL-2.0-or-later */
// Copyright 2020 Google LLC.

#ifndef __DRIVERS_SOC_QCOM_SD_TRAY_H__
#define __DRIVERS_SOC_QCOM_SD_TRAY_H__

#include "qcom_spmi.h"

GpioOps *new_qcom_sd_tray_cd_wrapper(GpioOps *sd_cd, QcomSpmi *pmic,
					uintptr_t offset, u32 val);

#endif

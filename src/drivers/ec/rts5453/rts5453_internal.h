// SPDX-License-Identifier: GPL-2.0
/* Copyright 2024 Google LLC.  */

#include <stdbool.h>
#include <stdint.h>

#include "drivers/ec/common/pdc_utils.h"

bool rts545x_check_update_compatibility(pdc_fw_ver_t current, pdc_fw_ver_t new);

// SPDX-License-Identifier: GPL-2.0
/* Copyright 2024 Google LLC.  */

#ifndef __DRIVERS_EC_RTS5453_INTERNAL_H__
#define __DRIVERS_EC_RTS5453_INTERNAL_H__

#include <stdbool.h>
#include <stdint.h>

#include "drivers/ec/common/pdc_utils.h"

bool rts545x_check_update_compatibility(pdc_fw_ver_t current, pdc_fw_ver_t new);

#endif /* __DRIVERS_EC_RTS5453_INTERNAL_H__ */

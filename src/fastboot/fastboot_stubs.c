// SPDX-License-Identifier: GPL-2.0
/* Copyright 2025 Google LLC. */

#include <libpayload.h>

#include "fastboot/fastboot.h"

/*
 * This stub is linked for fastboot function in non-developer builds or when
 * FASTBOOT_IN_PROD config is not selected. In developer build or when
 * FASTBOOT_IN_PROD is set, function from fastboot.c will override this stub.
 */

__weak void fastboot(void) { /* do nothing */ }

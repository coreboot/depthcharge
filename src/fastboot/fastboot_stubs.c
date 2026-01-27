// SPDX-License-Identifier: GPL-2.0
/* Copyright 2025 Google LLC. */

#include <libpayload.h>

#include "fastboot/fastboot.h"

/*
 * These stubs are linked for fastboot function in non-developer builds or when
 * FASTBOOT_IN_PROD config is not selected. In developer build or when
 * FASTBOOT_IN_PROD is set, functions from fastboot.c will override these stubs.
 */

__weak void fastboot(void) { /* do nothing */ }

__weak struct FastbootOps *fastboot_init(void) { return NULL; }
__weak bool fastboot_is_finished(struct FastbootOps *fb) { return true; }
__weak void fastboot_release(struct FastbootOps *fb_session) { /* do nothing */ }

__weak void fastboot_poll(struct FastbootOps *fb_session, uint32_t timeout_ms)
{
	/* do nothing */
}

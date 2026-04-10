// SPDX-License-Identifier: GPL-2.0
/* Copyright 2025 Google LLC. */

#include <libpayload.h>

#include "fastboot/fastboot.h"

/*
 * These stubs are linked for fastboot function in non-developer builds or when
 * FASTBOOT_IN_PROD config is not selected. In developer build or when
 * FASTBOOT_IN_PROD is set, functions from fastboot.c and log.c will override
 * these stubs.
 */

__weak void fastboot(void) { /* do nothing */ }

__weak uint64_t fastboot_log_get_total_bytes(const struct fastboot_log *log) { return 0; }
__weak uint64_t fastboot_log_get_oldest_available_byte(const struct fastboot_log *log)
{
	return 0;
}
__weak const char *fastboot_log_get_buf(const struct fastboot_log *log, uint64_t start,
					size_t *num)
{
	return NULL;
}
__weak void fastboot_log_drop_buf(const struct fastboot_log *log, const char *buf)
{
	/* do nothing */
}
__weak uint64_t fastboot_log_get_iter(const struct fastboot_log *log, uint64_t byte)
{
	return UINT64_MAX;
}
__weak bool fastboot_log_inc_iter(const struct fastboot_log *log, uint64_t *iter)
{
	return false;
}
__weak bool fastboot_log_dec_iter(const struct fastboot_log *log, uint64_t *iter)
{
	return false;
}
__weak char fastboot_log_get_byte_at_iter(const struct fastboot_log *log, uint64_t iter)
{
	return '\0';
}
__weak struct FastbootOps *fastboot_init(void) { return NULL; }
__weak bool fastboot_is_finished(struct FastbootOps *fb) { return true; }
__weak enum fastboot_state fastboot_release(struct FastbootOps *fb_session) { return FINISHED; }

__weak void fastboot_poll(struct FastbootOps *fb_session, uint32_t timeout_ms)
{
	/* do nothing */
}

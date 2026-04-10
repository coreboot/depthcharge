/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2026 Google LLC. */

#ifndef __FASTBOOT_LOG_H__
#define __FASTBOOT_LOG_H__

#include <limits.h>
#include <stddef.h>

#include "fastboot/fastboot.h"

/* Maximum number of bytes that log can contain */
#define FASTBOOT_LOG_BUF_SIZE (32 * KiB)

/* State of logger for messages generated while servicing fastboot messages */
struct fastboot_log {
	/* Storage for log */
	char buf[FASTBOOT_LOG_BUF_SIZE];
	/* Index where next byte should be written */
	uint64_t idx;
	/* Total number of written bytes (including already overwritten ones) */
	uint64_t total_len;
};

/*
 * Set logger that will collect print messages. If log is NULL, print messages are not
 * collected.
 */
void fastboot_log_set_active(struct fastboot_log *log);
/* Allocate and init log structure for fb */
void fastboot_log_init(struct FastbootOps *fb);
/* Free log structure of fb */
void fastboot_log_release(struct FastbootOps *fb);

#endif /* __FASTBOOT_LOG_H__ */

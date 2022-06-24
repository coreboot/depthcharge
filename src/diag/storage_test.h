/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2021 Google Inc.
 */

#ifndef __DIAG_STORAGE_TEST_H__
#define __DIAG_STORAGE_TEST_H__

#include <vb2_api.h>

#include "drivers/storage/blockdev.h"
#include "drivers/storage/info.h"

/* Prevent dumping storage self-test log too frequently */
#define DIAG_STORAGE_TEST_SHORT_DELAY_MS 200
#define DIAG_STORAGE_TEST_EXTENDED_DELAY_MS 3000
#define DIAG_STORAGE_TEST_DEFAULT_DELAY_MS 1000

/*
 * Dump storage self-test log into the buffer and truncate the text which
 * exceeds "end".
 *
 * The storage test log is retrieved from the block device driver. When this
 * happens too frequently, the block device might return unexpected result.
 * Therefore, this function uses a stopwatch to record the timestamp. If the
 * current call is too close to the previous one, DIAG_TEST_RUNNING will be
 * returned without retrieving the log.
 *
 * @param buf		The buffer to store the self-test log.
 * @param end		The pointer to the maximum limit of the buffer.
 *
 * @return VB2_SUCCESS on success, or other vb2_error_t on failure.
 */
vb2_error_t diag_dump_storage_test_log(char *buf, const char *end);

/*
 * Start or stop a storage self-test.
 *
 * @param ops		The self-test type.
 *
 * @return VB2_SUCCESS on success, or other vb2_error_t on failure.
 */
vb2_error_t diag_storage_test_control(enum BlockDevTestOpsType ops);

#endif

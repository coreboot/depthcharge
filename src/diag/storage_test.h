/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2021 Google LLC
 */

#ifndef __DIAG_STORAGE_TEST_H__
#define __DIAG_STORAGE_TEST_H__

#include "diag/common.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/info.h"

/* Prevent dumping storage self-test log too frequently */
#define DIAG_STORAGE_TEST_SHORT_DELAY_MS 200
#define DIAG_STORAGE_TEST_EXTENDED_DELAY_MS 3000
#define DIAG_STORAGE_TEST_DEFAULT_DELAY_MS 1000

/*
 * Check if storage self-test is supported on the first fixed block device.
 *
 * @return the bitmask of BlockDevTestOpsType.
 */
uint32_t diag_storage_test_supported(void);

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
 * @return DIAG_TEST_PASSED if the test finished and the test result is passed,
 * or other DiagTestResult values.
 */
DiagTestResult diag_dump_storage_test_log(char *buf, const char *end);

/*
 * Start or stop a storage self-test.
 *
 * @param ops		The self-test type.
 *
 * @return DIAG_TEST_PASSED if the test finished and the test result is passed,
 * or other DiagTestResult values.
 */
DiagTestResult diag_storage_test_control(enum BlockDevTestOpsType ops);

#endif

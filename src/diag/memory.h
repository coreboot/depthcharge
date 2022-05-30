/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2020 Google Inc.
 */

#ifndef __DIAG_MEMORY_H__
#define __DIAG_MEMORY_H__

#include "diag/common.h"

typedef enum {
	MEMORY_TEST_MODE_QUICK,
	MEMORY_TEST_MODE_FULL,
} MemoryTestMode;

/*
 * Initialize the memory test state with specific test mode.
 *
 * @return DIAG_TEST_SUCCESS on success, or other DiagTestResult values on
 * failure.
 */
DiagTestResult memory_test_init(MemoryTestMode mode);

/*
 * Run a batch of the test and return the current status to the read only buf.
 * Otherwise, if the test is finished, it will return the test result.
 *
 * Callee will handle the ownership of the buffer. The buffer will be available
 * until next call.
 *
 * @return DIAG_TEST_SUCCESS on success, or other DiagTestResult values.
 */
DiagTestResult memory_test_run(const char **buf);

#endif

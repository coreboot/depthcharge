/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2020 Google Inc.
 */

#ifndef __DIAG_MEMORY_H__
#define __DIAG_MEMORY_H__

#include <vb2_api.h>

typedef enum {
	MEMORY_TEST_MODE_QUICK,
	MEMORY_TEST_MODE_FULL,
} MemoryTestMode;

/*
 * Initialize the memory test state with specific test mode.
 *
 * Return VB2_SUCCESS if success, VB2_ERROR_EX_DIAG_TEST_INIT_FAILED for failed.
 */
vb2_error_t memory_test_init(MemoryTestMode mode);

/*
 * Run a batch of the test and return the current status to the read only buf.
 * Otherwise, if the test is finished, it will return the test result.
 *
 * Callee will handle the ownership of the buffer. The buffer will be available
 * until next call.
 *
 * Return VB2_SUCCESS if the test finished. VB2_ERROR_EX_DIAG_TEST_RUNNING if
 * the test is still running but the buffer was unchanged.
 * VB2_ERROR_EX_DIAG_TEST_UPDATED if the test is still running and the buffer
 * was updated. Other for other errors.
 */
vb2_error_t memory_test_run(const char **buf);

#endif

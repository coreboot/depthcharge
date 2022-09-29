/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __DIAG_COMMON_H__
#define __DIAG_COMMON_H__

typedef enum {
	/* The test finished without error; the test result is passed. */
	DIAG_TEST_PASSED = 0,
	/* The test finished without error; the test result is failed. */
	DIAG_TEST_FAILED,
	/* The test encountered some internal error and stopped. */
	DIAG_TEST_ERROR,
	/* The test is not implemented. */
	DIAG_TEST_UNIMPLEMENTED,
	/* The test is running but the output buffer was unchanged. */
	DIAG_TEST_RUNNING,
	/* The test is running and the output buffer was updated. */
	DIAG_TEST_UPDATED,
} DiagTestResult;

#endif

/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __DIAG_COMMON_H__
#define __DIAG_COMMON_H__

typedef enum {
	DIAG_TEST_SUCCESS = 0,
	DIAG_TEST_ERROR,
	DIAG_TEST_UNIMPLEMENTED,
	/* The test is running but the output buffer was unchanged. */
	DIAG_TEST_RUNNING,
	/* The test is running and the output buffer was updated. */
	DIAG_TEST_UPDATED,
} DiagTestResult;

#endif

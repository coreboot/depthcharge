/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __DIAG_COMMON_H__
#define __DIAG_COMMON_H__

#include <libpayload.h>

/* The return type of a test chunk. The caller could use this value to decide if
   the callee finished all the test chunks or not. */
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

/******************************************************************************/
/* APIs for diagnostics reports */

/*
 * Notify the diagnostic report service to start a new test item.
 * If there is another ongoing test, mark it as error and start a new one.
 *
 * @return 0 on success, -1 on error.
 */
int diag_report_start_test(uint8_t type);

/*
 * Notify the diagnostic report service to finish the current test item and add
 * an event to the report.
 * If no test is running, do nothing.
 *
 * @return 0 on success, -1 on error.
 */
int diag_report_end_test(uint8_t result);

/*
 * Dump all the events in the diagnostics report to the buffer in reversed
 * chronological order until the buffer is full. All the remaining events will
 * be dropped.
 * The events are saved in elog_event_cros_diag_log format.
 *
 * @return The number of bytes written into the buffer.
 */
size_t diag_report_dump(void *buf, size_t size);

/* Clear all the events in the report. */
void diag_report_clear(void);

#endif

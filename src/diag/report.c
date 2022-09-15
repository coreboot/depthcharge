// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "diag/common.h"
#include "diag/diag_internal.h"
#include "drivers/timer/timer.h"

/* The type and start time of the current running test event */
static uint8_t current_type = ELOG_CROS_DIAG_TYPE_NONE;
static uint64_t current_start_time_us;

/* Use a ring buffer to keep latest test events, from oldest to latest. */
#define EVENT_NEXT(idx)							       \
	(((idx) + 1) % DIAG_REPORT_EVENT_MAX)
#define EVENT_PREV(idx)							       \
	(((idx) + DIAG_REPORT_EVENT_MAX - 1) % DIAG_REPORT_EVENT_MAX)
static union elog_event_cros_diag_log report_events[DIAG_REPORT_EVENT_MAX];
static int report_events_front, report_events_rear;
static size_t report_events_count;

static void reset_current(void)
{
	current_type = ELOG_CROS_DIAG_TYPE_NONE;
	current_start_time_us = 0;
}

static int diag_report_push_event(uint8_t type, uint8_t result,
				  uint64_t time_us)
{
	uint16_t time_s;

	/* We have 16-bit limit for time_s so for the events with time > MAX,
	   we have to save them as time = MAX instead. */
	time_s = (uint16_t)MIN(time_us / USECS_PER_SEC, UINT16_MAX);

	/* Validate the type and result. */
	if (type > DIAG_REPORT_EVENT_TYPE_MAX ||
	    result > DIAG_REPORT_EVENT_RESULT_MAX) {
		printf("%s: Invalid type=%u, result=%u\n",
		       __func__, type, result);
		return -1;
	}

	/* The events are full, remove the oldest one from the report */
	if (report_events_count == DIAG_REPORT_EVENT_MAX) {
		report_events_front = EVENT_NEXT(report_events_front);
		report_events_count--;
	}

	printf("%s: type=%u result=%u time=%um%us\n", __func__,
	       type, result, time_s / 60, time_s % 60);

	report_events[report_events_rear].type = type;
	report_events[report_events_rear].result = result;
	report_events[report_events_rear].time_s = time_s;
	report_events_rear = EVENT_NEXT(report_events_rear);
	report_events_count++;

	return 0;
}

int diag_report_start_test(uint8_t type)
{
	/* If there is another unfinished test item, mark it as an error. */
	diag_report_end_test(ELOG_CROS_DIAG_RESULT_ERROR);

	/* Validate the type code. */
	if (type > DIAG_REPORT_EVENT_TYPE_MAX) {
		printf("%s: Invalid type=%u\n", __func__, type);
		return -1;
	}

	current_type = type;
	current_start_time_us = timer_us(0);
	return 0;
}

int diag_report_end_test(uint8_t result)
{
	int rv;

	/* No current test item, return success. */
	if (current_type == ELOG_CROS_DIAG_TYPE_NONE)
		return 0;

	/* Validate the result code. */
	if (result > DIAG_REPORT_EVENT_RESULT_MAX) {
		printf("%s: Invalid result=%u\n", __func__, result);
		return -1;
	}

	rv = diag_report_push_event(current_type, result,
				    timer_us(current_start_time_us));
	reset_current();
	return rv;
}

size_t diag_report_dump(void *buf, size_t len)
{
	size_t offset, raw_size, dump_count;
	int report_events_now;

	/* If there is another unfinished test item, mark it as an error. */
	diag_report_end_test(ELOG_CROS_DIAG_RESULT_ERROR);

	memset(buf, 0, len);
	offset = 0;
	raw_size = sizeof(report_events[0].raw);
	/* Dump from the latest report. */
	report_events_now = EVENT_PREV(report_events_rear);
	for (dump_count = 0; dump_count < report_events_count; dump_count++) {
		if (offset + raw_size > len) {
			printf("%s: The buffer is too small; truncated\n",
			       __func__);
			break;
		}
		memcpy(buf + offset, &report_events[report_events_now].raw,
		       raw_size);
		offset += raw_size;
		report_events_now = EVENT_PREV(report_events_now);
	}
	printf("%s: Dump %zu diagnostics report event(s); total %zu\n",
	       __func__, dump_count, report_events_count);
	return offset;
}

void diag_report_clear(void)
{
	reset_current();
	memset(report_events, 0, sizeof(report_events));
	report_events_front = 0;
	report_events_rear = 0;
	report_events_count = 0;
}

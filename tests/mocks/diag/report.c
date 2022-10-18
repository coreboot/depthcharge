// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "diag/common.h"
#include "tests/test.h"

static uint8_t current_type = ELOG_CROS_DIAG_TYPE_NONE;

int diag_report_start_test(uint8_t type)
{
	diag_report_end_test(ELOG_CROS_DIAG_RESULT_ERROR);
	check_expected(type);
	current_type = type;
	return 0;
}

int diag_report_end_test(uint8_t result)
{
	if (current_type == ELOG_CROS_DIAG_TYPE_NONE)
		return 0;
	check_expected(result);
	current_type = ELOG_CROS_DIAG_TYPE_NONE;
	return 0;
}

size_t diag_report_dump(void *buf, size_t size)
{
	void *data = mock_type(void *);
	size_t data_size = mock_type(size_t);

	if (data_size > size)
		fail_msg("The mocked report data exceeds maximum buffer size");

	memset(buf, 0, size);
	memcpy(buf, data, data_size);

	check_expected(size);

	return data_size;
}

void diag_report_clear(void)
{
	current_type = ELOG_CROS_DIAG_TYPE_NONE;
}

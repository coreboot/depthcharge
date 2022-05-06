// SPDX-License-Identifier: GPL-2.0

#include <mocks/callbacks.h>
#include <tests/test.h>
#include <tests/vboot/ui/common.h>
#include <vboot/ui.h>

/*
 * This mock function requires a mock value as expected called time, and has two
 * check_expected calls for the two parameters. The expected_time checking is
 * done by ASSERT_TIME_RANGE.
 */
void ui_beep(uint32_t msec, uint32_t frequency)
{
	uint32_t expected_time = mock_type(uint32_t);

	check_expected(msec);
	check_expected(frequency);
	if (expected_time != MOCK_IGNORE)
		ASSERT_TIME_RANGE(mock_time_ms, expected_time);

	mock_time_ms += msec;
}

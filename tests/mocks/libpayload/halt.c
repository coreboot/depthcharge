// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <libpayload.h>

void halt(void)
{
	/*
	 * Failing asserts are jumping to cmocka or user code - depending on
	 * previous expect_assert_failure() - and do not return.
	 */
	mock_assert(0, "halt", __FILE__, __LINE__);
	/* Infinte loop is necessary due to __attribute__((noreturn)) */
	while (1);
}

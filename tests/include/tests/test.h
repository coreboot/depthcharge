/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _TESTS_TEST_H
#define _TESTS_TEST_H

/*
 * Standard test header that should be included in all tests. For now it just
 * encapsulates the include dependencies for Cmocka. Test-specific APIs that are
 * so generic we would want them available everywhere could also be added here.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

/*
 * CMocka defines uintptr_t by default. This creates a conflict with stdint.h
 * typedef in depthcharge. Define below prevents conflicts by informing CMocka,
 * that uintptr_t is already defined.
 */
#define _UINTPTR_T_DEFINED
#include <cmocka.h>

/* Ensure that die()/halt() is called. */
#define expect_die(expression) expect_assert_failure(expression)

/*
 * CMocka doesn't support printing custom message on assertion failure. When a
 * helper macro has multiple assertion calls (such as ASSERT_SCREEN_STATE), it's
 * impossible for us to tell the failing call because the file names and line
 * numbers are the same for these calls. The following macro will print given
 * message and fail the test when the given integers are not equal.
 */
#define assert_int_equal_msg(a, b, msg) \
	do { \
		intmax_t _assert_local_a = (a), _assert_local_b = (b); \
		if (_assert_local_a != _assert_local_b) { \
			fail_msg("%#llx != %#llx: %s", _assert_local_a, \
				 _assert_local_b, (msg)); \
		} \
	} while (0)

/*
 * Set symbol value and make it global.
 */
#define TEST_SYMBOL(symbol, value) \
	asm(".set " #symbol ", " #value "\n\t.globl " #symbol)

/*
 * Define memory region for testing purpose.
 *
 * Create buffer with specified name and size.
 * Create end symbol for it.
 */
#define TEST_REGION(region, size) \
	__weak extern uint8_t _##region[]; \
	__weak extern uint8_t _e##region[]; \
	uint8_t _##region[size]; \
	TEST_SYMBOL(_e##region, _##region + size); \
	TEST_SYMBOL(_##region##_size, size)

#endif /* _TESTS_TEST_H */

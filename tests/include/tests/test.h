/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _TESTS_TEST_H
#define _TESTS_TEST_H

/*
 * Standard test header that should be included in all tests. For now it just
 * encapsulates the include dependencies for Cmocka. Test-specific APIs that are
 * so generic we would want them available everywhere could also be added here.
 */

#include <ctype.h>
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

/* Assert a is less than b. */
#define assert_int_less_than(a, b) \
	do { \
		intmax_t _assert_local_a = (a), _assert_local_b = (b); \
		if (_assert_local_a >= _assert_local_b) \
			fail_msg("%#llx is not less than %#llx", \
				 _assert_local_a, \
				 _assert_local_b); \
	} while (0)

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
 * When testing functions that copy buffers around, an easy way to get test data
 * is to memset() the source buffer to a certain byte value, and then check if
 * all bytes of the destination buffer contain that value after execution of the
 * code under test. This helper can perform that check.
 */
#define assert_filled_with(buffer, c, size) do { \
	const char *_buffer = (const char *)(buffer); \
	const char _c = (c); \
	size_t _size = (size); \
	for (size_t _i = 0; _i < _size; _i++) if (_buffer[_i] != _c) \
		fail_msg("(" #buffer ")[%#zx] == 0x%.2x ('%c') != 0x%.2x ('%c')", \
			 _i, _buffer[_i], isprint(_buffer[_i]) ? _buffer[_i] : '?', \
			 _c, isprint(_c) ? _c : '?'); \
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

#define EMPTY_WRAP(...) __VA_ARGS__

#endif /* _TESTS_TEST_H */

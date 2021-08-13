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

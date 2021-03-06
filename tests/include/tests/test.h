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

#endif /* _TESTS_TEST_H */

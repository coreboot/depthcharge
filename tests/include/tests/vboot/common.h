/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _TESTS_VBOOT_COMMON_H
#define _TESTS_VBOOT_COMMON_H

#include <tests/test.h>
#include <vb2_api.h>

#define ASSERT_VB2_SUCCESS(expr) assert_int_equal((expr), VB2_SUCCESS)

#endif /* _TESTS_VBOOT_COMMON_H */

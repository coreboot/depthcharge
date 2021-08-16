/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _TESTS_VBOOT_COMMON_H
#define _TESTS_VBOOT_COMMON_H

#include <tests/test.h>
#include <vboot_api.h>
#include <vb2_api.h>

#define ASSERT_VB2_SUCCESS(expr) assert_int_equal((expr), VB2_SUCCESS)

/*
 * Add return value to VbExKeyboardReadWithFlags.
 * Set VB_KEY_FLAG_TRUSTED_KEYBOARD flag if the passed trusted is non-zero.
 */
#define WILL_PRESS_KEY(key, trusted) \
	do { \
		will_return(VbExKeyboardReadWithFlags, \
			    (trusted) ? VB_KEY_FLAG_TRUSTED_KEYBOARD : 0); \
		will_return(VbExKeyboardReadWithFlags, (key)); \
	} while (0)

/*
 * Add will_return to VbExIsShutdownRequested. VbExIsShutdownRequested will
 * 1. Return 0 for iterations - 1 times, and then
 * 2. Return 1 once.
 */
#define WILL_SHUTDOWN_IN(iterations) \
	do { \
		if ((iterations) > 1) \
			will_return_count(VbExIsShutdownRequested, 0, \
					  (iterations) - 1); \
		will_return(VbExIsShutdownRequested, 1); \
	} while (0)

/* Add return value to vb2ex_physical_presence_pressed. */
#define WILL_PRESS_PHYSICAL_PRESENCE(pressed) \
	will_return(vb2ex_physical_presence_pressed, (pressed))

#endif /* _TESTS_VBOOT_COMMON_H */

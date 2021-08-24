/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _TESTS_VBOOT_UI_COMMON_H
#define _TESTS_VBOOT_UI_COMMON_H

#include <tests/test.h>
#include <vb2_api.h>
#include <vboot/ui.h>

/* Fixed value for ignoring some checks. */
#define MOCK_IGNORE 0xffffu

#define ASSERT_SCREEN_STATE(_state, _screen, _selected_item, \
			    _hidden_item_mask) \
	do { \
		if ((_screen) != MOCK_IGNORE) { \
			assert_non_null((_state)->screen); \
			assert_int_equal_msg((_state)->screen->id, (_screen), \
					     "screen"); \
		} \
		if ((_selected_item) != MOCK_IGNORE) \
			assert_int_equal_msg((_state)->selected_item, \
					     (_selected_item), \
					     "selected_item"); \
		if ((_hidden_item_mask) != MOCK_IGNORE) \
			assert_int_equal_msg((_state)->hidden_item_mask, \
					     (_hidden_item_mask), \
					     "hidden_item_mask"); \
	} while (0)

#endif /* _TESTS_VBOOT_UI_COMMON_H */

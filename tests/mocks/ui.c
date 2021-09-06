// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <vboot/ui.h>

uint32_t ui_keyboard_read(uint32_t *key_flags)
{
	*key_flags = mock_type(uint32_t);
	return mock_type(uint32_t);
}

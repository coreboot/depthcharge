// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <vboot/ui.h>

int ui_is_power_pressed(void)
{
	return 0;
}

int ui_is_physical_presence_pressed(void)
{
	return mock();
}
